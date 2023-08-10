/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // Dependencies
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const { span } = require("devtools/client/shared/vendor/react-dom-factories");

  const {
    lengthBubble,
  } = require("devtools/client/shared/components/reps/shared/grip-length-bubble");
  const {
    interleave,
    getGripType,
    wrapRender,
    ellipsisElement,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");
  const {
    MODE,
  } = require("devtools/client/shared/components/reps/reps/constants");

  const DEFAULT_TITLE = "Array";

  /**
   * Renders an array. The array is enclosed by left and right bracket
   * and the max number of rendered items depends on the current mode.
   */

  GripArray.propTypes = {
    object: PropTypes.object.isRequired,
    mode: PropTypes.oneOf(Object.values(MODE)),
    provider: PropTypes.object,
    onDOMNodeMouseOver: PropTypes.func,
    onDOMNodeMouseOut: PropTypes.func,
    onInspectIconClick: PropTypes.func,
    shouldRenderTooltip: PropTypes.bool,
  };

  function GripArray(props) {
    const { object, mode = MODE.SHORT, shouldRenderTooltip } = props;

    let brackets;
    const needSpace = function (space) {
      return space ? { left: "[ ", right: " ]" } : { left: "[", right: "]" };
    };

    const config = {
      "data-link-actor-id": object.actor,
      className: "objectBox objectBox-array",
      title: shouldRenderTooltip ? "Array" : null,
    };

    const title = getTitle(props, object);

    if (mode === MODE.TINY) {
      const isEmpty = getLength(object) === 0;

      // Omit bracketed ellipsis for non-empty non-Array arraylikes (f.e: Sets).
      if (!isEmpty && object.class !== "Array") {
        return span(config, title);
      }

      brackets = needSpace(false);
      return span(
        config,
        title,
        span(
          {
            className: "arrayLeftBracket",
          },
          brackets.left
        ),
        isEmpty ? null : ellipsisElement,
        span(
          {
            className: "arrayRightBracket",
          },
          brackets.right
        )
      );
    }

    if (mode === MODE.HEADER) {
      return span(config, title);
    }

    const max = maxLengthMap.get(mode);
    const items = arrayIterator(props, object, max);
    brackets = needSpace(!!items.length);

    return span(
      config,
      title,
      span(
        {
          className: "arrayLeftBracket",
        },
        brackets.left
      ),
      ...interleave(items, ", "),
      span(
        {
          className: "arrayRightBracket",
        },
        brackets.right
      ),
      span({
        className: "arrayProperties",
        role: "group",
      })
    );
  }

  function getLength(grip) {
    if (!grip.preview) {
      return 0;
    }

    return grip.preview.length || grip.preview.childNodesLength || 0;
  }

  function getTitle(props, object) {
    const objectLength = getLength(object);
    const isEmpty = objectLength === 0;

    let title = props.title || object.class || DEFAULT_TITLE;

    const length = lengthBubble({
      object,
      mode: props.mode,
      maxLengthMap,
      getLength,
    });

    if (props.mode === MODE.TINY) {
      if (isEmpty) {
        if (object.class === DEFAULT_TITLE) {
          return null;
        }

        return span({ className: "objectTitle" }, `${title} `);
      }

      let trailingSpace;
      if (object.class === DEFAULT_TITLE) {
        title = null;
        trailingSpace = " ";
      }

      return span({ className: "objectTitle" }, title, length, trailingSpace);
    }

    if (props.mode === MODE.HEADER) {
      return span({ className: "objectTitle" }, title, length);
    }

    return span({ className: "objectTitle" }, title, length, " ");
  }

  function getPreviewItems(grip) {
    if (!grip.preview) {
      return null;
    }

    return grip.preview.items || grip.preview.childNodes || [];
  }

  function arrayIterator(props, grip, max) {
    const { Rep } = require("devtools/client/shared/components/reps/reps/rep");

    let items = [];
    const gripLength = getLength(grip);

    if (!gripLength) {
      return items;
    }

    const previewItems = getPreviewItems(grip);
    const provider = props.provider;

    let emptySlots = 0;
    let foldedEmptySlots = 0;
    items = previewItems.reduce((res, itemGrip) => {
      if (res.length >= max) {
        return res;
      }

      let object;
      try {
        if (!provider && itemGrip === null) {
          emptySlots++;
          return res;
        }

        object = provider ? provider.getValue(itemGrip) : itemGrip;
      } catch (exc) {
        object = exc;
      }

      if (emptySlots > 0) {
        res.push(getEmptySlotsElement(emptySlots));
        foldedEmptySlots = foldedEmptySlots + emptySlots - 1;
        emptySlots = 0;
      }

      if (res.length < max) {
        res.push(
          Rep({
            ...props,
            object,
            mode: MODE.TINY,
            // Do not propagate title to array items reps
            title: undefined,
          })
        );
      }

      return res;
    }, []);

    // Handle trailing empty slots if there are some.
    if (items.length < max && emptySlots > 0) {
      items.push(getEmptySlotsElement(emptySlots));
      foldedEmptySlots = foldedEmptySlots + emptySlots - 1;
    }

    const itemsShown = items.length + foldedEmptySlots;
    if (gripLength > itemsShown) {
      items.push(ellipsisElement);
    }

    return items;
  }

  function getEmptySlotsElement(number) {
    // TODO: Use l10N - See https://github.com/firefox-devtools/reps/issues/141
    return `<${number} empty slot${number > 1 ? "s" : ""}>`;
  }

  function supportsObject(grip, noGrip = false) {
    return (
      grip?.preview &&
      (grip.preview.kind == "ArrayLike" ||
        getGripType(grip, noGrip) === "DocumentFragment")
    );
  }

  const maxLengthMap = new Map();
  maxLengthMap.set(MODE.SHORT, 3);
  maxLengthMap.set(MODE.LONG, 10);

  // Exports from this module
  module.exports = {
    rep: wrapRender(GripArray),
    supportsObject,
    maxLengthMap,
    getLength,
  };
});
