/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  SymbolActor,
} = require("resource://devtools/server/actors/object/symbol.js");

function run_test() {
  test_SA_destroy();
  test_SA_form();
  test_SA_raw();
}

const SYMBOL_NAME = "abc";
const TEST_SYMBOL = Symbol(SYMBOL_NAME);

function makeMockSymbolActor() {
  const symbol = TEST_SYMBOL;
  const mockConn = null;
  const actor = new SymbolActor(mockConn, symbol);
  actor.actorID = "symbol1";
  const parentPool = {
    symbolActors: {
      [symbol]: actor,
    },
    unmanage: () => {},
  };
  actor.getParent = () => parentPool;
  return actor;
}

function test_SA_destroy() {
  const actor = makeMockSymbolActor();
  strictEqual(actor.getParent().symbolActors[TEST_SYMBOL], actor);

  actor.destroy();
  strictEqual(TEST_SYMBOL in actor.getParent().symbolActors, false);
}

function test_SA_form() {
  const actor = makeMockSymbolActor();
  const form = actor.form();
  strictEqual(form.type, "symbol");
  strictEqual(form.actor, actor.actorID);
  strictEqual(form.name, SYMBOL_NAME);
}

function test_SA_raw() {
  const actor = makeMockSymbolActor();
  strictEqual(actor.rawValue(), TEST_SYMBOL);
}
