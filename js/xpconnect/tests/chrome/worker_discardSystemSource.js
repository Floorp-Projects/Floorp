function canary() {
  var someBitOfSource = 42;
}

postMessage(canary.toSource());
