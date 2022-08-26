/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function(require, exports, module) {
  // Dependencies
  const { span } = require("devtools/client/shared/vendor/react-dom-factories");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const {
    wrapRender,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");
  const {
    MODE,
  } = require("devtools/client/shared/components/reps/reps/constants");

  const ModePropType = PropTypes.oneOf(Object.values(MODE));

  /**
   * Renders an array. The array is enclosed by left and right bracket
   * and the max number of rendered items depends on the current mode.
   */

  ArrayRep.propTypes = {
    mode: ModePropType,
    object: PropTypes.array.isRequired,
    shouldRenderTooltip: PropTypes.bool,
  };

  function ArrayRep(props) {
    const { object, mode = MODE.SHORT, shouldRenderTooltip = true } = props;

    let brackets;
    let items;
    const needSpace = function(space) {
      return space ? { left: "[ ", right: " ]" } : { left: "[", right: "]" };
    };

    if (mode === MODE.TINY) {
      const isEmpty = object.length === 0;
      if (isEmpty) {
        items = [];
      } else {
        items = [
          span(
            {
              className: "more-ellipsis",
            },
            "…"
          ),
        ];
      }
      brackets = needSpace(false);
    } else {
      items = arrayIterator(props, object, maxLengthMap.get(mode));
      brackets = needSpace(!!items.length);
    }

    return span(
      {
        className: "objectBox objectBox-array",
        title: shouldRenderTooltip ? "Array" : null,
      },
      span(
        {
          className: "arrayLeftBracket",
        },
        brackets.left
      ),
      ...items,
      span(
        {
          className: "arrayRightBracket",
        },
        brackets.right
      )
    );
  }

  function arrayIterator(props, array, max) {
    const items = [];

    for (let i = 0; i < array.length && i < max; i++) {
      const config = {
        mode: MODE.TINY,
        delim: i == array.length - 1 ? "" : ", ",
      };
      let item;

      try {
        item = ItemRep({
          ...props,
          ...config,
          object: array[i],
        });
      } catch (exc) {
        item = ItemRep({
          ...props,
          ...config,
          object: exc,
        });
      }
      items.push(item);
    }

    if (array.length > max) {
      items.push(
        span(
          {
            className: "more-ellipsis",
          },
          "…"
        )
      );
    }

    return items;
  }

  /**
   * Renders array item. Individual values are separated by a comma.
   */

  ItemRep.propTypes = {
    object: PropTypes.any.isRequired,
    delim: PropTypes.string.isRequired,
    mode: ModePropType,
  };

  function ItemRep(props) {
    const { Rep } = require("devtools/client/shared/components/reps/reps/rep");

    const { object, delim, mode } = props;
    return span(
      {},
      Rep({
        ...props,
        object,
        mode,
      }),
      delim
    );
  }

  function getLength(object) {
    return object.length;
  }

  function supportsObject(object, noGrip = false) {
    return (
      noGrip &&
      (Array.isArray(object) ||
        Object.prototype.toString.call(object) === "[object Arguments]")
    );
  }

  const maxLengthMap = new Map();
  maxLengthMap.set(MODE.SHORT, 3);
  maxLengthMap.set(MODE.LONG, 10);

  // Exports from this module
  module.exports = {
    rep: wrapRender(ArrayRep),
    supportsObject,
    maxLengthMap,
    getLength,
  };
});
