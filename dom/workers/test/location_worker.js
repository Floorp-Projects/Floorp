/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
for (var string in self.location) {
  var value = typeof self.location[string] === "function"
              ? self.location[string]()
              : self.location[string];
  postMessage({ "string": string, "value": value });
}
postMessage({ "string": "testfinished" });
