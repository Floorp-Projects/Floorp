/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { RetVal, types } = require("resource://devtools/shared/protocol.js");

// Predeclare the domstylerule actor type
types.addActorType("domstylerule");

/**
 * When asking for the styles applied to a node, we return a list of
 * appliedstyle json objects that lists the rules that apply to the node
 * and which element they were inherited from (if any).
 *
 * Note appliedstyle only sends the list of actorIDs and is not a valid return
 * value on its own. appliedstyle should be returned with the actual list of
 * StyleRuleActor and StyleSheetActor. See appliedStylesReturn.
 */
types.addDictType("appliedstyle", {
  rule: "domstylerule#actorid",
  inherited: "nullable:domnode#actorid",
  keyframes: "nullable:domstylerule#actorid",
});

types.addDictType("matchedselector", {
  rule: "domstylerule#actorid",
  selector: "string",
  value: "string",
  status: "number",
});

types.addDictType("appliedStylesReturn", {
  entries: "array:appliedstyle",
  rules: "array:domstylerule",
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
