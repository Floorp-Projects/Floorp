/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { Actor, ActorClassWithSpec } = require("devtools/shared/protocol/Actor");
var { Pool } = require("devtools/shared/protocol/Pool");
var {
  types,
  registerFront,
  getFront,
  createRootFront,
} = require("devtools/shared/protocol/types");
var { Front } = require("devtools/shared/protocol/Front");
var {
  FrontClassWithSpec,
} = require("devtools/shared/protocol/Front/FrontClassWithSpec");
var { Arg, Option } = require("devtools/shared/protocol/Request");
const { RetVal } = require("devtools/shared/protocol/Response");
const {
  generateActorSpec,
} = require("devtools/shared/protocol/Actor/generateActorSpec");

exports.Front = Front;
exports.Pool = Pool;
exports.Actor = Actor;
exports.ActorClassWithSpec = ActorClassWithSpec;
exports.types = types;
exports.generateActorSpec = generateActorSpec;
exports.FrontClassWithSpec = FrontClassWithSpec;
exports.Arg = Arg;
exports.Option = Option;
exports.RetVal = RetVal;
exports.registerFront = registerFront;
exports.getFront = getFront;
exports.createRootFront = createRootFront;
