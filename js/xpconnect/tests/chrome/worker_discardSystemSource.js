function canary() {
  // eslint-disable-next-line no-unused-vars
  var someBitOfSource = 42;
}

postMessage(canary.toString());
