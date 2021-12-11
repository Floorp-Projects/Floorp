/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function(require, exports, module) {
  // Dependencies
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const { span } = require("devtools/client/shared/vendor/react-dom-factories");
  // Utils
  const {
    wrapRender,
  } = require("devtools/client/shared/components/reps/reps/rep-utils");
  const PropRep = require("devtools/client/shared/components/reps/reps/prop-rep");
  const {
    MODE,
  } = require("devtools/client/shared/components/reps/reps/constants");
  /**
   * Renders an map entry. A map entry is represented by its key,
   * a column and its value.
   *
   * tooltipTitle Notes:
   * ---
   * 1. Renders a Map Entry.
   * 2. Implements tooltipTitle: <TODO>
   * 3. ElementTitle = <TODO>
   *      POTENTIAL: full key/value pair for display
   * 4. Chrome: chrome displays full key-value pair
   */

  GripMapEntry.propTypes = {
    object: PropTypes.object,
    // @TODO Change this to Object.values when supported in Node's version of V8
    mode: PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
    onDOMNodeMouseOver: PropTypes.func,
    onDOMNodeMouseOut: PropTypes.func,
    onInspectIconClick: PropTypes.func,
  };

  function GripMapEntry(props) {
    const { object } = props;

    let { key, value } = object.preview;
    if (key && key.getGrip) {
      key = key.getGrip();
    }
    if (value && value.getGrip) {
      value = value.getGrip();
    }

    return span(
      {
        className: "objectBox objectBox-map-entry",
      },
      PropRep({
        ...props,
        name: key,
        object: value,
        equal: " \u2192 ",
        title: null,
        suppressQuotes: false,
      })
    );
  }

  function supportsObject(grip, noGrip = false) {
    if (noGrip === true) {
      return false;
    }
    return (
      grip &&
      (grip.type === "mapEntry" || grip.type === "storageEntry") &&
      grip.preview
    );
  }

  function createGripMapEntry(key, value) {
    return {
      type: "mapEntry",
      preview: {
        key,
        value,
      },
    };
  }

  // Exports from this module
  module.exports = {
    rep: wrapRender(GripMapEntry),
    createGripMapEntry,
    supportsObject,
  };
});
