/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function(require, exports, module) {
  // ReactJS
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const { span } = require("devtools/client/shared/vendor/react-dom-factories");

  // Dependencies
  const {
    getGripType,
    isGrip,
    wrapRender,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");

  const PropRep = require("devtools/client/shared/components/reps/reps/prop-rep");
  const {
    MODE,
  } = require("devtools/client/shared/components/reps/reps/constants");

  /**
   * Renders a DOM Promise object.
   */

  PromiseRep.propTypes = {
    object: PropTypes.object.isRequired,
    // @TODO Change this to Object.values when supported in Node's version of V8
    mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
    onDOMNodeMouseOver: PropTypes.func,
    onDOMNodeMouseOut: PropTypes.func,
    onInspectIconClick: PropTypes.func,
    shouldRenderTooltip: PropTypes.bool,
  };

  function PromiseRep(props) {
    const object = props.object;
    const shouldRenderTooltip = props.shouldRenderTooltip;
    const { promiseState } = object;

    const config = {
      "data-link-actor-id": object.actor,
      className: "objectBox objectBox-object",
      title: shouldRenderTooltip ? "Promise" : null,
    };

    if (props.mode === MODE.TINY) {
      const {
        Rep,
      } = require("devtools/client/shared/components/reps/reps/rep");

      return span(
        config,
        getTitle(object),
        span(
          {
            className: "objectLeftBrace",
          },
          " { "
        ),
        Rep({ object: promiseState.state }),
        span(
          {
            className: "objectRightBrace",
          },
          " }"
        )
      );
    }

    const propsArray = getProps(props, promiseState);
    return span(
      config,
      getTitle(object),
      span(
        {
          className: "objectLeftBrace",
        },
        " { "
      ),
      ...propsArray,
      span(
        {
          className: "objectRightBrace",
        },
        " }"
      )
    );
  }

  function getTitle(object) {
    return span(
      {
        className: "objectTitle",
      },
      object.class
    );
  }

  function getProps(props, promiseState) {
    const keys = ["state"];
    if (promiseState.hasOwnProperty("value")) {
      keys.push("value");
    } else if (promiseState.hasOwnProperty("reason")) {
      keys.push("reason");
    }

    return keys.reduce((res, key, i) => {
      const object = promiseState[key];
      res = res.concat(
        PropRep({
          ...props,
          mode: MODE.TINY,
          name: `<${key}>`,
          object: object.getGrip ? object.getGrip() : object,
          equal: ": ",
          suppressQuotes: true,
        })
      );

      // Interleave commas between elements
      if (i !== keys.length - 1) {
        res.push(", ");
      }

      return res;
    }, []);
  }

  // Registration
  function supportsObject(object, noGrip = false) {
    if (noGrip === true || !isGrip(object)) {
      return false;
    }
    return getGripType(object, noGrip) == "Promise";
  }

  // Exports from this module
  module.exports = {
    rep: wrapRender(PromiseRep),
    supportsObject,
  };
});
