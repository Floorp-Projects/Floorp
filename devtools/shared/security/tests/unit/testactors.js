/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { RootActor } = require("devtools/server/actors/root");
const {
  ActorRegistry,
} = require("devtools/server/actors/utils/actor-registry");

exports.createRootActor = function createRootActor(connection) {
  const root = new RootActor(connection, {
    globalActorFactories: ActorRegistry.globalActorFactories,
  });
  root.applicationType = "xpcshell-tests";
  return root;
};
