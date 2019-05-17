/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

require("./reps.css");

// Load all existing rep templates
const Undefined = require("./undefined");
const Null = require("./null");
const StringRep = require("./string");
const Number = require("./number");
const ArrayRep = require("./array");
const Obj = require("./object");
const SymbolRep = require("./symbol");
const InfinityRep = require("./infinity");
const NaNRep = require("./nan");
const Accessor = require("./accessor");

// DOM types (grips)
const Accessible = require("./accessible");
const Attribute = require("./attribute");
const BigInt = require("./big-int");
const DateTime = require("./date-time");
const Document = require("./document");
const DocumentType = require("./document-type");
const Event = require("./event");
const Func = require("./function");
const PromiseRep = require("./promise");
const RegExp = require("./regexp");
const StyleSheet = require("./stylesheet");
const CommentNode = require("./comment-node");
const ElementNode = require("./element-node");
const TextNode = require("./text-node");
const ErrorRep = require("./error");
const Window = require("./window");
const ObjectWithText = require("./object-with-text");
const ObjectWithURL = require("./object-with-url");
const GripArray = require("./grip-array");
const GripMap = require("./grip-map");
const GripMapEntry = require("./grip-map-entry");
const Grip = require("./grip");

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
  ArrayRep,
  Document,
  DocumentType,
  Window,
  ObjectWithText,
  ObjectWithURL,
  ErrorRep,
  GripArray,
  GripMap,
  GripMapEntry,
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
  Obj,
];

/**
 * Generic rep that is used for rendering native JS types or an object.
 * The right template used for rendering is picked automatically according
 * to the current value type. The value must be passed in as the 'object'
 * property.
 */
const Rep = function(props) {
  const { object, defaultRep } = props;
  const rep = getRep(object, defaultRep, props.noGrip);
  return rep(props);
};

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
 */
function getRep(object, defaultRep = Grip, noGrip = false) {
  for (let i = 0; i < reps.length; i++) {
    const rep = reps[i];
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
  REPS: {
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
    GripMapEntry,
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
  },
  // Exporting for tests
  getRep,
};
