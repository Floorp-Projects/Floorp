function test () {
  let loader = makeLoader();
  let module = Module("./main", gTestPath);
  let require = Require(loader, module);

  // sdk/util/uuid uses Cc, Ci, components
  const { uuid } = require("sdk/util/uuid");

  ok(isUUID(uuid()), "chrome.Cc and chrome.Ci works in SDK includes");

  let uuidString = '00001111-2222-3333-4444-555566667777';
  let parsed = uuid(uuidString);
  is(parsed, '{' + uuidString + '}', "chrome.components works in SDK includes");

  // sdk/base64 uses Cu
  const { encode } = require("sdk/base64");
  is(encode("hello"), "aGVsbG8=", "chrome.Cu works in SDK includes");
  finish();
}
