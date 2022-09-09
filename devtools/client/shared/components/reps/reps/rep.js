/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Make this available to both AMD and CJS environments
define(function(require, exports, module) {
  // Load all existing rep templates
  const Undefined = require("devtools/client/shared/components/reps/reps/undefined");
  const Null = require("devtools/client/shared/components/reps/reps/null");
  const StringRep = require("devtools/client/shared/components/reps/reps/string");
  const Number = require("devtools/client/shared/components/reps/reps/number");
  const ArrayRep = require("devtools/client/shared/components/reps/reps/array");
  const Obj = require("devtools/client/shared/components/reps/reps/object");
  const SymbolRep = require("devtools/client/shared/components/reps/reps/symbol");
  const InfinityRep = require("devtools/client/shared/components/reps/reps/infinity");
  const NaNRep = require("devtools/client/shared/components/reps/reps/nan");
  const Accessor = require("devtools/client/shared/components/reps/reps/accessor");

  // DOM types (grips)
  const Accessible = require("devtools/client/shared/components/reps/reps/accessible");
  const Attribute = require("devtools/client/shared/components/reps/reps/attribute");
  const BigInt = require("devtools/client/shared/components/reps/reps/big-int");
  const DateTime = require("devtools/client/shared/components/reps/reps/date-time");
  const Document = require("devtools/client/shared/components/reps/reps/document");
  const DocumentType = require("devtools/client/shared/components/reps/reps/document-type");
  const Event = require("devtools/client/shared/components/reps/reps/event");
  const Func = require("devtools/client/shared/components/reps/reps/function");
  const PromiseRep = require("devtools/client/shared/components/reps/reps/promise");
  const RegExp = require("devtools/client/shared/components/reps/reps/regexp");
  const StyleSheet = require("devtools/client/shared/components/reps/reps/stylesheet");
  const CommentNode = require("devtools/client/shared/components/reps/reps/comment-node");
  const ElementNode = require("devtools/client/shared/components/reps/reps/element-node");
  const TextNode = require("devtools/client/shared/components/reps/reps/text-node");
  const ErrorRep = require("devtools/client/shared/components/reps/reps/error");
  const Window = require("devtools/client/shared/components/reps/reps/window");
  const ObjectWithText = require("devtools/client/shared/components/reps/reps/object-with-text");
  const ObjectWithURL = require("devtools/client/shared/components/reps/reps/object-with-url");
  const GripArray = require("devtools/client/shared/components/reps/reps/grip-array");
  const GripEntry = require("devtools/client/shared/components/reps/reps/grip-entry");
  const GripMap = require("devtools/client/shared/components/reps/reps/grip-map");
  const Grip = require("devtools/client/shared/components/reps/reps/grip");

  // List of all registered template.
  // XXX there should be a way for extensions to register a new
  // or modify an existing rep.
  const reps = [
    RegExp,
    StyleSheet,
    Event,
    DateTime,
    CommentNode,
    Accessible,
    ElementNode,
    TextNode,
    Attribute,
    Func,
    PromiseRep,
    Document,
    DocumentType,
    Window,
    ObjectWithText,
    ObjectWithURL,
    ErrorRep,
    GripArray,
    GripMap,
    GripEntry,
    Grip,
    Undefined,
    Null,
    StringRep,
    Number,
    BigInt,
    SymbolRep,
    InfinityRep,
    NaNRep,
    Accessor,
  ];

  // Reps for rendering of native object reference (e.g. used from the JSONViewer, Netmonitor, …)
  const noGripReps = [StringRep, Number, ArrayRep, Undefined, Null, Obj];

  /**
   * Generic rep that is used for rendering native JS types or an object.
   * The right template used for rendering is picked automatically according
   * to the current value type. The value must be passed in as the 'object'
   * property.
   */
  const Rep = function(props) {
    const { object, defaultRep } = props;
    const rep = getRep(
      object,
      defaultRep,
      props.noGrip,
      props.mayUseCustomFormatter
    );
    return rep(props);
  };

  const exportedReps = {
    Accessible,
    Accessor,
    ArrayRep,
    Attribute,
    BigInt,
    CommentNode,
    DateTime,
    Document,
    DocumentType,
    ElementNode,
    ErrorRep,
    Event,
    Func,
    Grip,
    GripArray,
    GripMap,
    GripEntry,
    InfinityRep,
    NaNRep,
    Null,
    Number,
    Obj,
    ObjectWithText,
    ObjectWithURL,
    PromiseRep,
    RegExp,
    Rep,
    StringRep,
    StyleSheet,
    SymbolRep,
    TextNode,
    Undefined,
    Window,
  };

  // Custom Formatters
  // ToDo: This preference can be removed once the custom formatters feature is stable enough
  // Services.prefs isn't available in jsonviewer. It doesn't matter as we don't want to use
  // custom formatters there
  if (typeof Services == "object" && Services?.prefs) {
    const customFormattersExperimentallyEnabled = Services.prefs.getBoolPref(
      "devtools.custom-formatters",
      false
    );

    const useCustomFormatters =
      customFormattersExperimentallyEnabled &&
      Services.prefs.getBoolPref("devtools.custom-formatters.enabled", false);

    if (useCustomFormatters) {
      const JsonMl = require("devtools/client/shared/components/reps/reps/jsonml");
      reps.unshift(JsonMl);
      exportedReps.JsonMl = JsonMl;
    }
  }

  // Helpers

  /**
   * Return a rep object that is responsible for rendering given
   * object.
   *
   * @param object {Object} Object to be rendered in the UI. This
   * can be generic JS object as well as a grip (handle to a remote
   * debuggee object).
   *
   * @param defaultRep {React.Component} The default template
   * that should be used to render given object if none is found.
   *
   * @param noGrip {Boolean} If true, will only check reps not made for remote
   *                         objects.
   *
   * @param mayUseCustomFormatter {Boolean} If true, custom formatters are
   * allowed to be used as rep.
   */
  function getRep(
    object,
    defaultRep = Grip,
    noGrip = false,
    mayUseCustomFormatter = false
  ) {
    const repsList = noGrip ? noGripReps : reps;
    for (const rep of repsList) {
      if (rep === exportedReps.JsonMl && !mayUseCustomFormatter) {
        continue;
      }

      try {
        // supportsObject could return weight (not only true/false
        // but a number), which would allow to priorities templates and
        // support better extensibility.
        if (rep.supportsObject(object, noGrip)) {
          return rep.rep;
        }
      } catch (err) {
        console.error(err);
      }
    }

    return defaultRep.rep;
  }

  module.exports = {
    Rep,
    REPS: exportedReps,
    // Exporting for tests
    getRep,
  };
});
