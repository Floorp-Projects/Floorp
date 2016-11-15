/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Make this available to both AMD and CJS environments
define(function (require, exports, module) {
  // Dependencies
  const React = require("devtools/client/shared/vendor/react");

  const { isGrip } = require("./rep-utils");

  // Load all existing rep templates
  const { Undefined } = require("./undefined");
  const { Null } = require("./null");
  const { StringRep } = require("./string");
  const { LongStringRep } = require("./long-string");
  const { Number } = require("./number");
  const { ArrayRep } = require("./array");
  const { Obj } = require("./object");
  const { SymbolRep } = require("./symbol");
  const { InfinityRep } = require("./infinity");
  const { NaNRep } = require("./nan");

  // DOM types (grips)
  const { Attribute } = require("./attribute");
  const { DateTime } = require("./date-time");
  const { Document } = require("./document");
  const { Event } = require("./event");
  const { Func } = require("./function");
  const { PromiseRep } = require("./promise");
  const { RegExp } = require("./regexp");
  const { StyleSheet } = require("./stylesheet");
  const { CommentNode } = require("./comment-node");
  const { ElementNode } = require("./element-node");
  const { TextNode } = require("./text-node");
  const { ErrorRep } = require("./error");
  const { Window } = require("./window");
  const { ObjectWithText } = require("./object-with-text");
  const { ObjectWithURL } = require("./object-with-url");
  const { GripArray } = require("./grip-array");
  const { GripMap } = require("./grip-map");
  const { Grip } = require("./grip");

  // List of all registered template.
  // XXX there should be a way for extensions to register a new
  // or modify an existing rep.
  let reps = [
    RegExp,
    StyleSheet,
    Event,
    DateTime,
    CommentNode,
    ElementNode,
    TextNode,
    Attribute,
    LongStringRep,
    Func,
    PromiseRep,
    ArrayRep,
    Document,
    Window,
    ObjectWithText,
    ObjectWithURL,
    ErrorRep,
    GripArray,
    GripMap,
    Grip,
    Undefined,
    Null,
    StringRep,
    Number,
    SymbolRep,
    InfinityRep,
    NaNRep,
  ];

  /**
   * Generic rep that is using for rendering native JS types or an object.
   * The right template used for rendering is picked automatically according
   * to the current value type. The value must be passed is as 'object'
   * property.
   */
  const Rep = React.createClass({
    displayName: "Rep",

    propTypes: {
      object: React.PropTypes.any,
      defaultRep: React.PropTypes.object,
      mode: React.PropTypes.string
    },

    render: function () {
      let rep = getRep(this.props.object, this.props.defaultRep);
      return rep(this.props);
    },
  });

  // Helpers

  /**
   * Return a rep object that is responsible for rendering given
   * object.
   *
   * @param object {Object} Object to be rendered in the UI. This
   * can be generic JS object as well as a grip (handle to a remote
   * debuggee object).
   *
   * @param defaultObject {React.Component} The default template
   * that should be used to render given object if none is found.
   */
  function getRep(object, defaultRep = Obj) {
    let type = typeof object;
    if (type == "object" && object instanceof String) {
      type = "string";
    } else if (object && type == "object" && object.type) {
      type = object.type;
    }

    if (isGrip(object)) {
      type = object.class;
    }

    for (let i = 0; i < reps.length; i++) {
      let rep = reps[i];
      try {
        // supportsObject could return weight (not only true/false
        // but a number), which would allow to priorities templates and
        // support better extensibility.
        if (rep.supportsObject(object, type)) {
          return React.createFactory(rep.rep);
        }
      } catch (err) {
        console.error(err);
      }
    }

    return React.createFactory(defaultRep.rep);
  }

  // Exports from this module
  exports.Rep = Rep;
});
