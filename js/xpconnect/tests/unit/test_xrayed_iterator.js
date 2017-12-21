const Cu = Components.utils;
function run_test() {

  var toEval = [
    "var customIterator = {",
    "  _array: [6, 7, 8, 9]",
    "};",
    "customIterator[Symbol.iterator] = function* () {",
    "    for (var i = 0; i < this._array.length; ++i)",
    "      yield this._array[i];",
    "};"
  ].join('\n');

  function checkIterator(iterator) {
    var control = [6, 7, 8, 9];
    var i = 0;
    for (var item of iterator) {
      Assert.equal(item, control[i]);
      ++i;
    }
  }

  // First, try in our own scope.
  eval(toEval);
  checkIterator(customIterator);

  // Next, try a vanilla CCW.
  var sbChrome = Cu.Sandbox(this);
  Cu.evalInSandbox(toEval, sbChrome, '1.7');
  checkIterator(sbChrome.customIterator);

  // Finally, try an Xray waiver.
  var sbContent = Cu.Sandbox('http://www.example.com');
  Cu.evalInSandbox(toEval, sbContent, '1.7');
  checkIterator(Cu.waiveXrays(sbContent.customIterator));
}
