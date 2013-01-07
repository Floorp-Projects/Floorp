/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
var props = {
  'ctypes': 1,
  'OS': 1
};
for (var prop in props) {
  postMessage({ "prop": prop, "value": self[prop] });
}
postMessage({ "testfinished": 1 });
