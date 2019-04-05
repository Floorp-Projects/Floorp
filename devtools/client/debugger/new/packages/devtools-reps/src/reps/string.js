/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Dependencies
const PropTypes = require("prop-types");

const {
  containsURL,
  isURL,
  escapeString,
  getGripType,
  rawCropString,
  sanitizeString,
  wrapRender,
  isGrip,
  tokenSplitRegex,
  ELLIPSIS
} = require("./rep-utils");

const dom = require("react-dom-factories");
const { a, span } = dom;

/**
 * Renders a string. String value is enclosed within quotes.
 */
StringRep.propTypes = {
  useQuotes: PropTypes.bool,
  escapeWhitespace: PropTypes.bool,
  style: PropTypes.object,
  cropLimit: PropTypes.number.isRequired,
  member: PropTypes.object,
  object: PropTypes.object.isRequired,
  openLink: PropTypes.func,
  className: PropTypes.string,
  title: PropTypes.string,
  isInContentPage: PropTypes.bool
};

function StringRep(props) {
  const {
    className,
    style,
    cropLimit,
    object,
    useQuotes = true,
    escapeWhitespace = true,
    member,
    openLink,
    title,
    isInContentPage
  } = props;

  let text = object;

  const isLong = isLongString(object);
  const isOpen = member && member.open;
  const shouldCrop = !isOpen && cropLimit && text.length > cropLimit;

  if (isLong) {
    text = maybeCropLongString(
      {
        shouldCrop,
        cropLimit
      },
      text
    );

    const { fullText } = object;
    if (isOpen && fullText) {
      text = fullText;
    }
  }

  text = formatText(
    {
      useQuotes,
      escapeWhitespace
    },
    text
  );

  const config = getElementConfig({
    className,
    style,
    actor: object.actor,
    title
  });

  if (!isLong) {
    if (containsURL(text)) {
      return span(
        config,
        ...getLinkifiedElements(
          text,
          shouldCrop && cropLimit,
          openLink,
          isInContentPage
        )
      );
    }

    // Cropping of longString has been handled before formatting.
    text = maybeCropString(
      {
        isLong,
        shouldCrop,
        cropLimit
      },
      text
    );
  }

  return span(config, text);
}

function maybeCropLongString(opts, text) {
  const { shouldCrop, cropLimit } = opts;

  const { initial, length } = text;

  text = shouldCrop ? initial.substring(0, cropLimit) : initial;

  if (text.length < length) {
    text += ELLIPSIS;
  }

  return text;
}

function formatText(opts, text) {
  const { useQuotes, escapeWhitespace } = opts;

  return useQuotes
    ? escapeString(text, escapeWhitespace)
    : sanitizeString(text);
}

function getElementConfig(opts) {
  const { className, style, actor, title } = opts;

  const config = {};

  if (actor) {
    config["data-link-actor-id"] = actor;
  }

  if (title) {
    config.title = title;
  }

  const classNames = ["objectBox", "objectBox-string"];
  if (className) {
    classNames.push(className);
  }
  config.className = classNames.join(" ");

  if (style) {
    config.style = style;
  }

  return config;
}

function maybeCropString(opts, text) {
  const { shouldCrop, cropLimit } = opts;

  return shouldCrop ? rawCropString(text, cropLimit) : text;
}

/**
 * Get an array of the elements representing the string, cropped if needed,
 * with actual links.
 *
 * @param {String} text: The actual string to linkify.
 * @param {Integer | null} cropLimit
 * @param {Function} openLink: Function handling the link opening.
 * @param {Boolean} isInContentPage: pass true if the reps is rendered in
 *                                   the content page (e.g. in JSONViewer).
 * @returns {Array<String|ReactElement>}
 */
function getLinkifiedElements(text, cropLimit, openLink, isInContentPage) {
  const halfLimit = Math.ceil((cropLimit - ELLIPSIS.length) / 2);
  const startCropIndex = cropLimit ? halfLimit : null;
  const endCropIndex = cropLimit ? text.length - halfLimit : null;

  // As we walk through the tokens of the source string, we make sure to
  // preserve the original whitespace that separated the tokens.
  let currentIndex = 0;
  const items = [];
  for (const token of text.split(tokenSplitRegex)) {
    if (isURL(token)) {
      // Let's grab all the non-url strings before the link.
      const tokenStart = text.indexOf(token, currentIndex);
      let nonUrlText = text.slice(currentIndex, tokenStart);
      nonUrlText = getCroppedString(
        nonUrlText,
        currentIndex,
        startCropIndex,
        endCropIndex
      );
      if (nonUrlText) {
        items.push(nonUrlText);
      }

      // Update the index to match the beginning of the token.
      currentIndex = tokenStart;

      const linkText = getCroppedString(
        token,
        currentIndex,
        startCropIndex,
        endCropIndex
      );
      if (linkText) {
        items.push(
          a(
            {
              className: "url",
              title: token,
              draggable: false,
              // Because we don't want the link to be open in the current
              // panel's frame, we only render the href attribute if `openLink`
              // exists (so we can preventDefault) or if the reps will be
              // displayed in content page (e.g. in the JSONViewer).
              href: openLink || isInContentPage ? token : null,
              onClick: openLink
                ? e => {
                    e.preventDefault();
                    openLink(token, e);
                  }
                : null
            },
            linkText
          )
        );
      }

      currentIndex = tokenStart + token.length;
    }
  }

  // Clean up any non-URL text at the end of the source string,
  // i.e. not handled in the loop.
  if (currentIndex !== text.length) {
    let nonUrlText = text.slice(currentIndex, text.length);
    if (currentIndex < endCropIndex) {
      nonUrlText = getCroppedString(
        nonUrlText,
        currentIndex,
        startCropIndex,
        endCropIndex
      );
    }
    items.push(nonUrlText);
  }

  return items;
}

/**
 * Returns a cropped substring given an offset, start and end crop indices in a
 * parent string.
 *
 * @param {String} text: The substring to crop.
 * @param {Integer} offset: The offset corresponding to the index at which
 *                          the substring is in the parent string.
 * @param {Integer|null} startCropIndex: the index where the start of the crop
 *                                       should happen in the parent string.
 * @param {Integer|null} endCropIndex: the index where the end of the crop
 *                                     should happen in the parent string
 * @returns {String|null} The cropped substring, or null if the text is
 *                        completly cropped.
 */
function getCroppedString(text, offset = 0, startCropIndex, endCropIndex) {
  if (!startCropIndex) {
    return text;
  }

  const start = offset;
  const end = offset + text.length;

  const shouldBeVisible = !(start >= startCropIndex && end <= endCropIndex);
  if (!shouldBeVisible) {
    return null;
  }

  const shouldCropEnd = start < startCropIndex && end > startCropIndex;
  const shouldCropStart = start < endCropIndex && end > endCropIndex;
  if (shouldCropEnd) {
    const cutIndex = startCropIndex - start;
    return (
      text.substring(0, cutIndex) +
      ELLIPSIS +
      (shouldCropStart ? text.substring(endCropIndex - start) : "")
    );
  }

  if (shouldCropStart) {
    // The string should be cropped at the beginning.
    const cutIndex = endCropIndex - start;
    return text.substring(cutIndex);
  }

  return text;
}

function isLongString(object) {
  return object && object.type === "longString";
}

function supportsObject(object, noGrip = false) {
  if (noGrip === false && isGrip(object)) {
    return isLongString(object);
  }

  return getGripType(object, noGrip) == "string";
}

// Exports from this module

module.exports = {
  rep: wrapRender(StringRep),
  supportsObject,
  isLongString
};
