/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { Actor, ActorClassWithSpec } = require("devtools/shared/protocol/Actor");
var { Pool } = require("devtools/shared/protocol/Pool");
var {
  types,
  registeredTypes,
  registerFront,
  getFront,
} = require("devtools/shared/protocol/types");
var { method } = require("devtools/shared/protocol/utils");
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
exports.method = method;

exports.dumpActorSpec = function(type) {
  const actorSpec = type.actorSpec;
  const ret = {
    category: "actor",
    typeName: type.name,
    methods: [],
    events: {},
  };

  for (const _method of actorSpec.methods) {
    ret.methods.push({
      name: _method.name,
      release: _method.release || undefined,
      oneway: _method.oneway || undefined,
      request: _method.request.describe(),
      response: _method.response.describe(),
    });
  }

  if (actorSpec.events) {
    for (const [name, request] of actorSpec.events) {
      ret.events[name] = request.describe();
    }
  }

  JSON.stringify(ret);

  return ret;
};

exports.dumpProtocolSpec = function() {
  const ret = {
    types: {},
  };

  for (let [name, type] of registeredTypes) {
    // Force lazy instantiation if needed.
    type = types.getType(name);
    const category = type.category || undefined;
    if (category === "dict") {
      ret.types[name] = {
        category: "dict",
        typeName: name,
        specializations: type.specializations,
      };
    } else if (category === "actor") {
      ret.types[name] = exports.dumpActorSpec(type);
    }
  }

  return ret;
};
