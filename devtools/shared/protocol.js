/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { Actor } = require("resource://devtools/shared/protocol/Actor.js");
var { Pool } = require("resource://devtools/shared/protocol/Pool.js");
var {
  types,
  registerFront,
  getFront,
  createRootFront,
} = require("resource://devtools/shared/protocol/types.js");
var { Front } = require("resource://devtools/shared/protocol/Front.js");
var {
  FrontClassWithSpec,
} = require("resource://devtools/shared/protocol/Front/FrontClassWithSpec.js");
var { Arg, Option } = require("resource://devtools/shared/protocol/Request.js");
const { RetVal } = require("resource://devtools/shared/protocol/Response.js");
const {
  generateActorSpec,
} = require("resource://devtools/shared/protocol/Actor/generateActorSpec.js");

exports.Front = Front;
exports.Pool = Pool;
exports.Actor = Actor;
exports.types = types;
exports.generateActorSpec = generateActorSpec;
exports.FrontClassWithSpec = FrontClassWithSpec;
exports.Arg = Arg;
exports.Option = Option;
exports.RetVal = RetVal;
exports.registerFront = registerFront;
exports.getFront = getFront;
exports.createRootFront = createRootFront;
