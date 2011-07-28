/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
var data = [ -1, 0, 1, 1.5, null, undefined, true, false, "foo",
             "123456789012345", "1234567890123456", "12345678901234567"];

var str = "";
for (var i = 0; i < 30; i++) {
  data.push(str);
  str += i % 2 ? "b" : "a";
}

onmessage = function(event) {
  data.forEach(function(string) {
    var encoded = btoa(string);
    postMessage({ type: "btoa", value: encoded });
    postMessage({ type: "atob", value: atob(encoded) });
  });

  var threw;
  try {
    atob();
  }
  catch(e) {
    threw = true;
  }

  if (!threw) {
    throw "atob didn't throw when called without an argument!";
  }
  threw = false;

  try {
    btoa();
  }
  catch(e) {
    threw = true;
  }

  if (!threw) {
    throw "btoa didn't throw when called without an argument!";
  }

  postMessage({ type: "done" });
}
