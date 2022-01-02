/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

add_task(
  threadFrontTest(async ({ threadFront, debuggee }) => {
    const packet = await executeOnNextTickAndWaitForPause(
      eval_code,
      threadFront
    );

    const [grip] = packet.frame.arguments;

    const objectFront = threadFront.pauseGrip(grip);

    // Checks the result of enumProperties.
    let response = await objectFront.enumProperties({});
    await check_enum_properties(response);

    // Checks the result of enumSymbols.
    response = await objectFront.enumSymbols();
    await check_enum_symbols(response);

    await threadFront.resume();

    function eval_code() {
      debuggee.eval(
        function stopMe(arg1) {
          debugger;
        }.toString()
      );

      debuggee.eval(`
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

    async function check_enum_properties(iterator) {
      equal(iterator.count, 10, "iterator.count has the expected value");

      info("Check iterator.slice response for all properties");
      let sliceResponse = await iterator.slice(0, iterator.count);
      ok(
        sliceResponse &&
          Object.getOwnPropertyNames(sliceResponse).includes("ownProperties"),
        "The response object has an ownProperties property"
      );

      let { ownProperties } = sliceResponse;
      let names = Object.keys(ownProperties);
      equal(
        names.length,
        iterator.count,
        "The response has the expected number of properties"
      );
      for (let i = 0; i < names.length; i++) {
        const name = names[i];
        equal(name, `property_${i}_key`);
        equal(ownProperties[name].value, `property_${i}_value`);
      }

      info("Check iterator.all response");
      const allResponse = await iterator.all();
      deepEqual(
        allResponse,
        sliceResponse,
        "iterator.all response has the expected data"
      );

      info("Check iterator response for 2 properties only");
      sliceResponse = await iterator.slice(2, 2);
      ok(
        sliceResponse &&
          Object.getOwnPropertyNames(sliceResponse).includes("ownProperties"),
        "The response object has an ownProperties property"
      );

      ownProperties = sliceResponse.ownProperties;
      names = Object.keys(ownProperties);
      equal(
        names.length,
        2,
        "The response has the expected number of properties"
      );
      equal(names[0], `property_2_key`);
      equal(names[1], `property_3_key`);
      equal(ownProperties[names[0]].value, `property_2_value`);
      equal(ownProperties[names[1]].value, `property_3_value`);
    }

    async function check_enum_symbols(iterator) {
      equal(iterator.count, 13, "iterator.count has the expected value");

      info("Check iterator.slice response for all symbols");
      let sliceResponse = await iterator.slice(0, iterator.count);
      ok(
        sliceResponse &&
          Object.getOwnPropertyNames(sliceResponse).includes("ownSymbols"),
        "The response object has an ownSymbols property"
      );

      let { ownSymbols } = sliceResponse;
      equal(
        ownSymbols.length,
        iterator.count,
        "The response has the expected number of symbols"
      );
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
      equal(iteratorSymbol.descriptor.value.getGrip().class, "Function");

      info("Check iterator.all response");
      const allResponse = await iterator.all();
      deepEqual(
        allResponse,
        sliceResponse,
        "iterator.all response has the expected data"
      );

      info("Check iterator response for 2 symbols only");
      sliceResponse = await iterator.slice(9, 2);
      ok(
        sliceResponse &&
          Object.getOwnPropertyNames(sliceResponse).includes("ownSymbols"),
        "The response object has an ownSymbols property"
      );

      ownSymbols = sliceResponse.ownSymbols;
      equal(
        ownSymbols.length,
        2,
        "The response has the expected number of symbols"
      );
      equal(ownSymbols[0].name, "Symbol(symbol_9)");
      equal(ownSymbols[0].descriptor.value, "symbol_9_value");
      equal(ownSymbols[1].name, "Symbol()");
      equal(ownSymbols[1].descriptor.value, "first unnamed symbol");
    }
  })
);
