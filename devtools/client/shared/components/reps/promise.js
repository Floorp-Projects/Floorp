/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // ReactJS
  const React = require("devtools/client/shared/vendor/react");
  // Dependencies
  const {
    createFactories,
    isGrip,
    wrapRender,
  } = require("./rep-utils");

  const { PropRep } = createFactories(require("./prop-rep"));
  const { MODE } = require("./constants");
  // Shortcuts
  const { span } = React.DOM;

  /**
   * Renders a DOM Promise object.
   */
  const PromiseRep = React.createClass({
    displayName: "Promise",

    propTypes: {
      object: React.PropTypes.object.isRequired,
      // @TODO Change this to Object.values once it's supported in Node's version of V8
      mode: React.PropTypes.oneOf(Object.keys(MODE).map(key => MODE[key])),
      objectLink: React.PropTypes.func,
    },

    getTitle: function (object) {
      const title = object.class;
      if (this.props.objectLink) {
        return this.props.objectLink({
          object: object
        }, title);
      }
      return title;
    },

    getProps: function (promiseState) {
      const keys = ["state"];
      if (Object.keys(promiseState).includes("value")) {
        keys.push("value");
      }

      return keys.map((key, i) => {
        return PropRep(Object.assign({}, this.props, {
          mode: MODE.TINY,
          name: `<${key}>`,
          object: promiseState[key],
          equal: ": ",
          delim: i < keys.length - 1 ? ", " : ""
        }));
      });
    },

    render: wrapRender(function () {
      const object = this.props.object;
      const {promiseState} = object;
      let objectLink = this.props.objectLink || span;

      if (this.props.mode === MODE.TINY) {
        let { Rep } = createFactories(require("./rep"));

        return (
          span({className: "objectBox objectBox-object"},
            this.getTitle(object),
            objectLink({
              className: "objectLeftBrace",
              object: object
            }, " { "),
            Rep({object: promiseState.state}),
            objectLink({
              className: "objectRightBrace",
              object: object
            }, " }")
          )
        );
      }

      const props = this.getProps(promiseState);
      return (
        span({className: "objectBox objectBox-object"},
          this.getTitle(object),
          objectLink({
            className: "objectLeftBrace",
            object: object
          }, " { "),
          ...props,
          objectLink({
            className: "objectRightBrace",
            object: object
          }, " }")
        )
      );
    }),
  });

  // Registration
  function supportsObject(object, type) {
    if (!isGrip(object)) {
      return false;
    }
    return type === "Promise";
  }

  // Exports from this module
  exports.PromiseRep = {
    rep: PromiseRep,
    supportsObject: supportsObject
  };
});
