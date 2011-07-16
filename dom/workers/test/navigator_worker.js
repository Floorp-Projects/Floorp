/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
var supportedProps = [
  "appName",
  "appVersion",
  "platform",
  "userAgent"
];

for (var prop in navigator) {
  // Make sure the list is current!
  if (supportedProps.indexOf(prop) == -1) {
    throw "Navigator has the '" + prop + "' property that isn't in the list!";
  }
}

var obj;

for (var index = 0; index < supportedProps.length; index++) {
  var prop = supportedProps[index];

  if (typeof navigator[prop] == "undefined") {
    throw "Navigator has no '" + prop + "' property!";
  }

  obj = {
    name:  prop,
    value: navigator[prop]
  };

  postMessage(JSON.stringify(obj));
}

obj = {
  name: "testFinished"
};

postMessage(JSON.stringify(obj));
