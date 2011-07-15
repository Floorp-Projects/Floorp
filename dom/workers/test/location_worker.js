/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
for (var string in self.location) {
  postMessage({ "string": string, "value": self.location[string] });
}
dump(self.location + " \n");
postMessage({ "string": "testfinished", "value": self.location.toString() });
