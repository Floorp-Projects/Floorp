/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // ReactJS
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const { span } = require("devtools/client/shared/vendor/react-dom-factories");

  // Reps
  const {
    getGripType,
    wrapRender,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");

  /**
   * Used to render JS built-in Date() object.
   */

  DateTime.propTypes = {
    object: PropTypes.object.isRequired,
    shouldRenderTooltip: PropTypes.bool,
  };

  function DateTime(props) {
    const { object: grip, shouldRenderTooltip } = props;
    let date;
    try {
      const dateObject = new Date(grip.preview.timestamp);
      // Calling `toISOString` will throw if the date is invalid,
      // so we can render an `Invalid Date` element.
      dateObject.toISOString();

      const dateObjectString = dateObject.toString();

      const config = getElementConfig({
        grip,
        dateObjectString,
        shouldRenderTooltip,
      });

      date = span(
        config,
        getTitle(grip),
        span({ className: "Date" }, dateObjectString)
      );
    } catch (e) {
      date = span(
        {
          className: "objectBox",
          title: shouldRenderTooltip ? "Invalid Date" : null,
        },
        "Invalid Date"
      );
    }

    return date;
  }

  function getElementConfig(opts) {
    const { grip, dateObjectString, shouldRenderTooltip } = opts;

    return {
      "data-link-actor-id": grip.actor,
      className: "objectBox",
      title: shouldRenderTooltip ? `${grip.class} ${dateObjectString}` : null,
    };
  }

  // getTitle() is used to render the `Date ` before the stringified date object,
  // not to render the actual span "title".

  function getTitle(grip) {
    return span(
      {
        className: "objectTitle",
      },
      `${grip.class} `
    );
  }

  // Registration
  function supportsObject(grip, noGrip = false) {
    return getGripType(grip, noGrip) == "Date" && grip?.preview;
  }

  // Exports from this module
  module.exports = {
    rep: wrapRender(DateTime),
    supportsObject,
  };
});
