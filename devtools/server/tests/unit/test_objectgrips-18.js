/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

var gDebuggee;
var gClient;
var gThreadClient;
var gCallback;

function run_test() {
  run_test_with_server(DebuggerServer, function () {
    run_test_with_server(WorkerDebuggerServer, do_test_finished);
  });
  do_test_pending();
}

async function run_test_with_server(server, callback) {
  gCallback = callback;
  initTestDebuggerServer(server);
  gDebuggee = addTestGlobal("test-grips", server);
  gDebuggee.eval(function stopMe(arg1) {
    debugger;
  }.toString());

  gClient = new DebuggerClient(server.connectPipe());
  await gClient.connect();
  const [,, threadClient] = await attachTestTabAndResume(gClient, "test-grips");
  gThreadClient = threadClient;
  test_object_grip();
}

async function test_object_grip() {
  gThreadClient.addOneTimeListener("paused", async function (event, packet) {
    let [grip] = packet.frame.arguments;

    let objClient = gThreadClient.pauseGrip(grip);

    // Checks the result of enumProperties.
    let response = await objClient.enumProperties({});
    await check_enum_properties(response);

    // Checks the result of enumSymbols.
    response = await objClient.enumSymbols();
    await check_enum_symbols(response);

    await gThreadClient.resume();
    await gClient.close();
    gCallback();
  });

  gDebuggee.eval(`
    var obj = Array.from({length: 10})
      .reduce((res, _, i) => {
        res["property_" + i + "_key"] = "property_" + i + "_value";
        res[Symbol("symbol_" + i)] = "symbol_" + i + "_value";
        return res;
      }, {});

    obj[Symbol()] = "first unnamed symbol";
    obj[Symbol()] = "second unnamed symbol";
    obj[Symbol.iterator] = function* () {
      yield 1;
      yield 2;
    };

    stopMe(obj);
  `);
}

async function check_enum_properties(response) {
  info("Check enumProperties response");
  ok(response && Object.getOwnPropertyNames(response).includes("iterator"),
    "The response object has an iterator property");

  const {iterator} = response;
  equal(iterator.count, 10, "iterator.count has the expected value");

  info("Check iterator.slice response for all properties");
  let sliceResponse = await iterator.slice(0, iterator.count);
  ok(sliceResponse && Object.getOwnPropertyNames(sliceResponse).includes("ownProperties"),
    "The response object has an ownProperties property");

  let {ownProperties} = sliceResponse;
  let names = Object.keys(ownProperties);
  equal(names.length, iterator.count,
    "The response has the expected number of properties");
  for (let i = 0; i < names.length; i++) {
    const name = names[i];
    equal(name, `property_${i}_key`);
    equal(ownProperties[name].value, `property_${i}_value`);
  }

  info("Check iterator.all response");
  let allResponse = await iterator.all();
  deepEqual(allResponse, sliceResponse, "iterator.all response has the expected data");

  info("Check iterator response for 2 properties only");
  sliceResponse = await iterator.slice(2, 2);
  ok(sliceResponse && Object.getOwnPropertyNames(sliceResponse).includes("ownProperties"),
    "The response object has an ownProperties property");

  ownProperties = sliceResponse.ownProperties;
  names = Object.keys(ownProperties);
  equal(names.length, 2, "The response has the expected number of properties");
  equal(names[0], `property_2_key`);
  equal(names[1], `property_3_key`);
  equal(ownProperties[names[0]].value, `property_2_value`);
  equal(ownProperties[names[1]].value, `property_3_value`);
}

async function check_enum_symbols(response) {
  info("Check enumProperties response");
  ok(response && Object.getOwnPropertyNames(response).includes("iterator"),
    "The response object has an iterator property");

  const {iterator} = response;
  equal(iterator.count, 13, "iterator.count has the expected value");

  info("Check iterator.slice response for all symbols");
  let sliceResponse = await iterator.slice(0, iterator.count);
  ok(sliceResponse && Object.getOwnPropertyNames(sliceResponse).includes("ownSymbols"),
    "The response object has an ownSymbols property");

  let {ownSymbols} = sliceResponse;
  equal(ownSymbols.length, iterator.count,
    "The response has the expected number of symbols");
  for (let i = 0; i < 10; i++) {
    const symbol = ownSymbols[i];
    equal(symbol.name, `Symbol(symbol_${i})`);
    equal(symbol.descriptor.value, `symbol_${i}_value`);
  }
  const firstUnnamedSymbol = ownSymbols[10];
  equal(firstUnnamedSymbol.name, "Symbol()");
  equal(firstUnnamedSymbol.descriptor.value, "first unnamed symbol");

  const secondUnnamedSymbol = ownSymbols[11];
  equal(secondUnnamedSymbol.name, "Symbol()");
  equal(secondUnnamedSymbol.descriptor.value, "second unnamed symbol");

  const iteratorSymbol = ownSymbols[12];
  equal(iteratorSymbol.name, "Symbol(Symbol.iterator)");
  equal(iteratorSymbol.descriptor.value.class, "Function");

  info("Check iterator.all response");
  let allResponse = await iterator.all();
  deepEqual(allResponse, sliceResponse, "iterator.all response has the expected data");

  info("Check iterator response for 2 symbols only");
  sliceResponse = await iterator.slice(9, 2);
  ok(sliceResponse && Object.getOwnPropertyNames(sliceResponse).includes("ownSymbols"),
    "The response object has an ownSymbols property");

  ownSymbols = sliceResponse.ownSymbols;
  equal(ownSymbols.length, 2, "The response has the expected number of symbols");
  equal(ownSymbols[0].name, "Symbol(symbol_9)");
  equal(ownSymbols[0].descriptor.value, "symbol_9_value");
  equal(ownSymbols[1].name, "Symbol()");
  equal(ownSymbols[1].descriptor.value, "first unnamed symbol");
}
