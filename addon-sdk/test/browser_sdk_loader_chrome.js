function test () {
  let loader = makeLoader();
  let module = Module("./main", gTestPath);
  let require = Require(loader, module);

  const { Ci, Cc, Cu, components } = require("chrome");

  let { generateUUID } = Cc["@mozilla.org/uuid-generator;1"]
                         .getService(Ci.nsIUUIDGenerator);
  ok(isUUID(generateUUID()), "chrome.Cc and chrome.Ci works");
  
  let { ID: parseUUID } = components;
  let uuidString = "00001111-2222-3333-4444-555566667777";
  let parsed = parseUUID(uuidString);
  is(parsed, "{" + uuidString + "}", "chrome.components works");

  const { defer } = Cu.import("resource://gre/modules/Promise.jsm").Promise;
  let { promise, resolve } = defer();
  resolve(5);
  promise.then(val => {
    is(val, 5, "chrome.Cu works");
    finish();
  });
}
