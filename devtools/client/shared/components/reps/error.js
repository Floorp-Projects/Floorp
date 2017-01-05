/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // ReactJS
  const React = require("devtools/client/shared/vendor/react");
  // Utils
  const {
    isGrip,
    wrapRender,
  } = require("./rep-utils");
  const { MODE } = require("./constants");
  // Shortcuts
  const { span } = React.DOM;

  /**
   * Renders Error objects.
   */
  const ErrorRep = React.createClass({
    displayName: "Error",

    propTypes: {
      object: React.PropTypes.object.isRequired,
      // @TODO Change this to Object.values once it's supported in Node's version of V8
      mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
      objectLink: React.PropTypes.func,
    },

    render: wrapRender(function () {
      let object = this.props.object;
      let preview = object.preview;
      let name = preview && preview.name
        ? preview.name
        : "Error";

      let content = this.props.mode === MODE.TINY
        ? name
        : `${name}: ${preview.message}`;

      if (preview.stack && this.props.mode !== MODE.TINY) {
        /*
         * Since Reps are used in the JSON Viewer, we can't localize
         * the "Stack trace" label (defined in debugger.properties as
         * "variablesViewErrorStacktrace" property), until Bug 1317038 lands.
         */
        content = `${content}\nStack trace:\n${preview.stack}`;
      }

      let objectLink = this.props.objectLink || span;
      return (
        objectLink({object, className: "objectBox-stackTrace"},
          span({}, content)
        )
      );
    }),
  });

  // Registration
  function supportsObject(object, type) {
    if (!isGrip(object)) {
      return false;
    }
    return (object.preview && type === "Error");
  }

  // Exports from this module
  exports.ErrorRep = {
    rep: ErrorRep,
    supportsObject: supportsObject
  };
});
