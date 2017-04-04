var worker = new Worker("navigator_worker.js");

var is = window.parent.is;
var ok = window.parent.ok;
var SimpleTest = window.parent.SimpleTest;

worker.onmessage = function(event) {
  var args = JSON.parse(event.data);

  if (args.name == "testFinished") {
    SimpleTest.finish();
    return;
  }

  if (typeof navigator[args.name] == "undefined") {
    ok(false, "Navigator has no '" + args.name + "' property!");
    return;
  }

  if (args.name === "languages") {
    is(navigator.languages.toString(), args.value.toString(), "languages matches");
    return;
  }

  if (args.name === "storage") {
    is(typeof navigator.storage, typeof args.value, "storage type matches");
    return;
  }

  if (args.name === "connection") {
    is(typeof navigator.connection, typeof args.value, "connection type matches");
    return;
  }

  is(navigator[args.name], args.value,
     "Mismatched navigator string for " + args.name + "!");
};

worker.onerror = function(event) {
  ok(false, "Worker had an error: " + event.message);
  SimpleTest.finish();
}

var version = SpecialPowers.Cc["@mozilla.org/xre/app-info;1"].getService(SpecialPowers.Ci.nsIXULAppInfo).version;
var isNightly = version.endsWith("a1");
var isRelease = !version.includes("a");

worker.postMessage({ isNightly, isRelease });
