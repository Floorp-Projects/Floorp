/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { RootActor } = require("resource://devtools/server/actors/root.js");
const {
  ActorRegistry,
} = require("resource://devtools/server/actors/utils/actor-registry.js");

exports.createRootActor = function createRootActor(connection) {
  const root = new RootActor(connection, {
    globalActorFactories: ActorRegistry.globalActorFactories,
  });
  root.applicationType = "xpcshell-tests";
  return root;
};
