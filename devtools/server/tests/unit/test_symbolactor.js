/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SymbolActor } = require("devtools/server/actors/object/symbol");

function run_test() {
  test_SA_destroy();
  test_SA_form();
  test_SA_raw();
}

const SYMBOL_NAME = "abc";
const TEST_SYMBOL = Symbol(SYMBOL_NAME);

function makeMockSymbolActor() {
  let symbol = TEST_SYMBOL;
  let actor = new SymbolActor(symbol);
  actor.actorID = "symbol1";
  actor.registeredPool = {
    symbolActors: {
      [symbol]: actor
    }
  };
  return actor;
}

function test_SA_destroy() {
  let actor = makeMockSymbolActor();
  strictEqual(actor.registeredPool.symbolActors[TEST_SYMBOL], actor);

  actor.destroy();
  strictEqual(TEST_SYMBOL in actor.registeredPool.symbolActors, false);
}

function test_SA_form() {
  let actor = makeMockSymbolActor();
  let form = actor.form();
  strictEqual(form.type, "symbol");
  strictEqual(form.actor, actor.actorID);
  strictEqual(form.name, SYMBOL_NAME);
}

function test_SA_raw() {
  let actor = makeMockSymbolActor();
  strictEqual(actor.rawValue(), TEST_SYMBOL);
}
