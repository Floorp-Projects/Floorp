/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test devtools/client/netmonitor/src/utils/request-utils.js function
// |parseJSON| ensure that it correctly handles plain JSON, Base 64
// encoded JSON, and JSON that has XSSI protection prepended to it

"use strict";

const { require } = ChromeUtils.importESModule(
  "resource://devtools/shared/loader/Loader.sys.mjs"
);
const {
  parseJSON,
} = require("resource://devtools/client/netmonitor/src/utils/request-utils.js");

function run_test() {
  const validJSON = '{"item":{"subitem":true},"seconditem":"bar"}';
  const base64JSON = btoa(validJSON);
  const parsedJSON = { item: { subitem: true }, seconditem: "bar" };
  const googleStyleXSSI = ")]}'\n";
  const facebookStyleXSSI = "for (;;);";
  const notRealXSSIPrevention = "sdgijsdjg";
  const while1XSSIPrevention = "while(1)";

  const parsedValidJSON = parseJSON(validJSON);
  info(JSON.stringify(parsedValidJSON));
  ok(
    parsedValidJSON.json.item.subitem == parsedJSON.item.subitem &&
      parsedValidJSON.json.seconditem == parsedJSON.seconditem,
    "plain JSON is parsed correctly"
  );

  const parsedBase64JSON = parseJSON(base64JSON);
  ok(
    parsedBase64JSON.json.item.subitem === parsedJSON.item.subitem &&
      parsedBase64JSON.json.seconditem === parsedJSON.seconditem,
    "base64 encoded JSON is parsed correctly"
  );

  const parsedGoogleStyleXSSIJSON = parseJSON(googleStyleXSSI + validJSON);
  ok(
    parsedGoogleStyleXSSIJSON.strippedChars === googleStyleXSSI &&
      parsedGoogleStyleXSSIJSON.error === void 0 &&
      parsedGoogleStyleXSSIJSON.json.item.subitem === parsedJSON.item.subitem &&
      parsedGoogleStyleXSSIJSON.json.seconditem === parsedJSON.seconditem,
    "Google style XSSI sequence correctly removed and returned"
  );

  const parsedFacebookStyleXSSIJSON = parseJSON(facebookStyleXSSI + validJSON);
  ok(
    parsedFacebookStyleXSSIJSON.strippedChars === facebookStyleXSSI &&
      parsedFacebookStyleXSSIJSON.error === void 0 &&
      parsedFacebookStyleXSSIJSON.json.item.subitem ===
        parsedJSON.item.subitem &&
      parsedFacebookStyleXSSIJSON.json.seconditem === parsedJSON.seconditem,
    "Facebook style XSSI sequence correctly removed and returned"
  );

  const parsedWhileXSSIJSON = parseJSON(while1XSSIPrevention + validJSON);
  ok(
    parsedWhileXSSIJSON.strippedChars === while1XSSIPrevention &&
      parsedWhileXSSIJSON.error === void 0 &&
      parsedWhileXSSIJSON.json.item.subitem === parsedJSON.item.subitem &&
      parsedWhileXSSIJSON.json.seconditem === parsedJSON.seconditem,
    "While XSSI sequence correctly removed and returned"
  );
  const parsedInvalidJson = parseJSON(notRealXSSIPrevention + validJSON);
  ok(
    !parsedInvalidJson.json && !parsedInvalidJson.strippedChars,
    "Parsed invalid JSON does not return a data object or strippedChars"
  );
  equal(
    parsedInvalidJson.error.name,
    "SyntaxError",
    "Parsing invalid JSON yeilds a SyntaxError"
  );
}
