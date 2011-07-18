for (var string in self.location) {
  postMessage({ "string": string, "value": self.location[string] });
}
postMessage({"string": "testfinished", "value": self.location.toString()});
