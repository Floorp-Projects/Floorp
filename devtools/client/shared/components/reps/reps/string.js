/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function(require, exports, module) {
  // Dependencies
  const {
    a,
    span,
  } = require("devtools/client/shared/vendor/react-dom-factories");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

  const {
    containsURL,
    escapeString,
    getGripType,
    rawCropString,
    sanitizeString,
    wrapRender,
    ELLIPSIS,
    uneatLastUrlCharsRegex,
    urlRegex,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");

  /**
   * Renders a string. String value is enclosed within quotes.
   */

  StringRep.propTypes = {
    useQuotes: PropTypes.bool,
    escapeWhitespace: PropTypes.bool,
    style: PropTypes.object,
    cropLimit: PropTypes.number.isRequired,
    urlCropLimit: PropTypes.number,
    member: PropTypes.object,
    object: PropTypes.object.isRequired,
    openLink: PropTypes.func,
    className: PropTypes.string,
    title: PropTypes.string,
    isInContentPage: PropTypes.bool,
    shouldRenderTooltip: PropTypes.bool,
  };

  function StringRep(props) {
    const {
      className,
      style,
      cropLimit,
      urlCropLimit,
      object,
      useQuotes = true,
      escapeWhitespace = true,
      member,
      openLink,
      title,
      isInContentPage,
      transformEmptyString = false,
      shouldRenderTooltip,
    } = props;

    let text = object;
    const config = getElementConfig({
      className,
      style,
      actor: object.actor,
      title,
    });

    if (text == "" && transformEmptyString && !useQuotes) {
      return span(
        {
          ...config,
          title: "<empty string>",
          className: `${config.className} objectBox-empty-string`,
        },
        "<empty string>"
      );
    }

    const isLong = isLongString(object);
    const isOpen = member && member.open;
    const shouldCrop = !isOpen && cropLimit && text.length > cropLimit;

    if (isLong) {
      text = maybeCropLongString(
        {
          shouldCrop,
          cropLimit,
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
        escapeWhitespace,
      },
      text
    );

    if (shouldRenderTooltip) {
      config.title = text;
    }

    if (!isLong) {
      if (containsURL(text)) {
        return span(
          config,
          getLinkifiedElements({
            text,
            cropLimit: shouldCrop ? cropLimit : null,
            urlCropLimit,
            openLink,
            isInContentPage,
          })
        );
      }

      // Cropping of longString has been handled before formatting.
      text = maybeCropString(
        {
          isLong,
          shouldCrop,
          cropLimit,
        },
        text
      );
    }

    return span(config, text);
  }

  function maybeCropLongString(opts, object) {
    const { shouldCrop, cropLimit } = opts;

    const grip = object && object.getGrip ? object.getGrip() : object;
    const { initial, length } = grip;

    let text = shouldCrop ? initial.substring(0, cropLimit) : initial;

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
   * @param {Object} An options object of the following shape:
   *                 - text {String}: The actual string to linkify.
   *                 - cropLimit {Integer}: The limit to apply on the whole text.
   *                 - urlCropLimit {Integer}: The limit to apply on each URL.
   *                 - openLink {Function} openLink: Function handling the link
   *                                                 opening.
   *                 - isInContentPage {Boolean}: pass true if the reps is
   *                                              rendered in the content page
   *                                              (e.g. in JSONViewer).
   * @returns {Array<String|ReactElement>}
   */
  function getLinkifiedElements({
    text,
    cropLimit,
    urlCropLimit,
    openLink,
    isInContentPage,
  }) {
    const halfLimit = Math.ceil((cropLimit - ELLIPSIS.length) / 2);
    const startCropIndex = cropLimit ? halfLimit : null;
    const endCropIndex = cropLimit ? text.length - halfLimit : null;

    const items = [];
    let currentIndex = 0;
    let contentStart;
    while (true) {
      const url = urlRegex.exec(text);
      // Pick the regexp with the earlier content; index will always be zero.
      if (!url) {
        break;
      }
      contentStart = url.index + url[1].length;
      if (contentStart > 0) {
        const nonUrlText = text.substring(0, contentStart);
        items.push(
          getCroppedString(
            nonUrlText,
            currentIndex,
            startCropIndex,
            endCropIndex
          )
        );
      }

      // There are some final characters for a URL that are much more likely
      // to have been part of the enclosing text rather than the end of the
      // URL.
      let useUrl = url[2];
      const uneat = uneatLastUrlCharsRegex.exec(useUrl);
      if (uneat) {
        useUrl = useUrl.substring(0, uneat.index);
      }

      currentIndex = currentIndex + contentStart;
      let linkText = getCroppedString(
        useUrl,
        currentIndex,
        startCropIndex,
        endCropIndex
      );

      if (linkText) {
        if (urlCropLimit && useUrl.length > urlCropLimit) {
          const urlCropHalf = Math.ceil((urlCropLimit - ELLIPSIS.length) / 2);
          linkText = getCroppedString(
            useUrl,
            0,
            urlCropHalf,
            useUrl.length - urlCropHalf
          );
        }

        items.push(
          a(
            {
              key: `${useUrl}-${currentIndex}`,
              className: "url",
              title: useUrl,
              draggable: false,
              // Because we don't want the link to be open in the current
              // panel's frame, we only render the href attribute if `openLink`
              // exists (so we can preventDefault) or if the reps will be
              // displayed in content page (e.g. in the JSONViewer).
              href: openLink || isInContentPage ? useUrl : null,
              target: "_blank",
              rel: "noopener noreferrer",
              onClick: openLink
                ? e => {
                    e.preventDefault();
                    openLink(useUrl, e);
                  }
                : null,
            },
            linkText
          )
        );
      }

      currentIndex = currentIndex + useUrl.length;
      text = text.substring(url.index + url[1].length + useUrl.length);
    }

    // Clean up any non-URL text at the end of the source string,
    // i.e. not handled in the loop.
    if (text.length) {
      if (currentIndex < endCropIndex) {
        text = getCroppedString(
          text,
          currentIndex,
          startCropIndex,
          endCropIndex
        );
      }
      items.push(text);
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
    const grip = object && object.getGrip ? object.getGrip() : object;
    return grip && grip.type === "longString";
  }

  function supportsObject(object, noGrip = false) {
    // Accept the object if the grip-type (or type for noGrip objects) is "string"
    if (getGripType(object, noGrip) == "string") {
      return true;
    }

    // Also accept longString objects if we're expecting grip
    if (!noGrip) {
      return isLongString(object);
    }

    return false;
  }

  // Exports from this module

  module.exports = {
    rep: wrapRender(StringRep),
    supportsObject,
    isLongString,
  };
});
