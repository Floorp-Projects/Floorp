/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { RetVal, types } = require("resource://devtools/shared/protocol.js");

// Predeclare the domstylerule actor type
const domstyleruleType = types.addActorType("domstylerule");

// @backward-compat { version 117 } The domstyleruleOrActorid type is only needed to handle
// older server as some properties. When 117 hits release, it can be removed
const domstyleruleactoridType = types.getType("domstylerule#actorid");
types.addType("appliedstyledomstyleruleOrActorid", {
  write: (value, context, detail) => {
    // This write function will only be used on newer server, and so we can always write
    // a proper domstylerule.
    return domstyleruleType.write(value, context, detail);
  },
  read: (value, context, detail) => {
    // When reading values, we might get a string, as older version of the server used
    // to only return an actorId.
    // In such case, we read with the domstylerule#actorid (that's what was used before)
    if (typeof value === "string") {
      return domstyleruleactoridType.read(value, context, detail);
    }
    // Otherwise, we can read it with the domstylerule type
    return domstyleruleType.read(value, context, detail);
  },
});

/**
 * When asking for the styles applied to a node, we return a list of
 * appliedstyle json objects that lists the rules that apply to the node
 * and which element they were inherited from (if any).
 */
types.addDictType("appliedstyle", {
  // @backward-compat { version 117 } When 117 hits release,
  // this should use the "domstylerule" type
  rule: "appliedstyledomstyleruleOrActorid",
  inherited: "nullable:domnode#actorid",
  // @backward-compat { version 117 } When 117 hits release,
  // this should use the "nullable:domstylerule" type
  keyframes: "nullable:appliedstyledomstyleruleOrActorid",
});

types.addDictType("matchedselector", {
  rule: "domstylerule#actorid",
  selector: "string",
  value: "string",
  status: "number",
});

types.addDictType("appliedStylesReturn", {
  entries: "array:appliedstyle",
  // @backward-compat { version 117 } Newer server does not return this "rules" property.
  // We still need it though as older server send it, and they also send only actor ids
  // for some properties in "entries". Those can only be turned into functioning fronts
  // if the actor form is properly handled at some point, which is why we need to keep
  // this "rules" property here.
  // Once 117 hits release, this can be removed.
  rules: "nullable:array:domstylerule",
});

types.addDictType("modifiedStylesReturn", {
  isMatching: RetVal("boolean"),
  ruleProps: RetVal("nullable:appliedStylesReturn"),
});

types.addDictType("fontpreview", {
  data: "nullable:longstring",
  size: "json",
});

types.addDictType("fontvariationaxis", {
  tag: "string",
  name: "string",
  minValue: "number",
  maxValue: "number",
  defaultValue: "number",
});

types.addDictType("fontvariationinstancevalue", {
  axis: "string",
  value: "number",
});

types.addDictType("fontvariationinstance", {
  name: "string",
  values: "array:fontvariationinstancevalue",
});

types.addDictType("fontface", {
  name: "string",
  CSSFamilyName: "string",
  rule: "nullable:domstylerule",
  srcIndex: "number",
  URI: "string",
  format: "string",
  preview: "nullable:fontpreview",
  localName: "string",
  metadata: "string",
  variationAxes: "array:fontvariationaxis",
  variationInstances: "array:fontvariationinstance",
});
