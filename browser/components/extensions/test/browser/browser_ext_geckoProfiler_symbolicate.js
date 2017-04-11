/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

let getExtension = () => {
  return ExtensionTestUtils.loadExtension({
    background: async function() {
      const [addresses, indices, strings] = await browser.geckoProfiler.getSymbols("test.pdb",
                                                                                   "ASDFQWERTY");

      function getSymbolAtAddress(address) {
        const index = addresses.indexOf(address);
        if (index == -1) {
          return null;
        }

        const nameBuffer = strings.subarray(indices[index], indices[index + 1]);
        const decoder = new TextDecoder("utf-8");

        return decoder.decode(nameBuffer);
      }

      browser.test.assertEq(getSymbolAtAddress(0x3fc74), "test_public_symbol", "Contains public symbol");
      browser.test.assertEq(getSymbolAtAddress(0x40330), "test_func_symbol", "Contains func symbol");
      browser.test.sendMessage("symbolicated");
    },

    manifest: {
      "permissions": ["geckoProfiler"],
      "applications": {
        "gecko": {
          "id": "profilertest@mozilla.com",
        },
      },
    },
  });
};

add_task(function* testProfilerControl() {
  SpecialPowers.pushPrefEnv({
    set: [
      [
        "extensions.geckoProfiler.symbols.url",
        "http://mochi.test:8888/browser/browser/components/extensions/test/browser/profilerSymbols.sjs?path=",
      ],
      [
        "extensions.geckoProfiler.acceptedExtensionIds",
        "profilertest@mozilla.com",
      ],
    ],
  });

  let extension = getExtension();
  yield extension.startup();
  yield extension.awaitMessage("symbolicated");
  yield extension.unload();
});
