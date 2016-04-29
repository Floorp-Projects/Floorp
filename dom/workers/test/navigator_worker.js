/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// IMPORTANT: Do not change the list below without review from a DOM peer!
var supportedProps = [
  "appCodeName",
  "appName",
  "appVersion",
  "platform",
  "product",
  "userAgent",
  "onLine",
  "language",
  "languages",
  "hardwareConcurrency",
];

self.onmessage = function(event) {
  if (!event || !event.data) {
    return;
  }

  startTest(event.data.isB2G);
};

function startTest(isB2G) {
  // Prepare the interface map showing if a propery should exist in this build.
  // For example, if interfaceMap[foo] = true means navigator.foo should exist.
  var interfaceMap = {};

  for (var prop of supportedProps) {
    if (typeof(prop) === "string") {
      interfaceMap[prop] = true;
      continue;
    }

    if (prop.b2g === !isB2G) {
      interfaceMap[prop.name] = false;
      continue;
    }

    interfaceMap[prop.name] = true;
  }

  for (var prop in navigator) {
    // Make sure the list is current!
    if (!interfaceMap[prop]) {
      throw "Navigator has the '" + prop + "' property that isn't in the list!";
    }
  }

  var obj;

  for (var prop in interfaceMap) {
    // Skip the property that is not supposed to exist in this build.
    if (!interfaceMap[prop]) {
      continue;
    }

    if (typeof navigator[prop] == "undefined") {
      throw "Navigator has no '" + prop + "' property!";
    }

    obj = { name:  prop };
    obj.value = navigator[prop];

    postMessage(JSON.stringify(obj));
  }

  obj = {
    name: "testFinished"
  };

  postMessage(JSON.stringify(obj));
}
