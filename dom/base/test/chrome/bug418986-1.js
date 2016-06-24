// The main test function.
var test = function (isContent) {
  SimpleTest.waitForExplicitFinish();

  let { ww } = SpecialPowers.Services;
  window.chromeWindow = ww.activeWindow;

  // The pairs of values expected to be the same when
  // fingerprinting resistance is enabled.
  let pairs = [
    ["screenX", 0],
    ["screenY", 0],
    ["mozInnerScreenX", 0],
    ["mozInnerScreenY", 0],
    ["screen.pixelDepth", 24],
    ["screen.colorDepth", 24],
    ["screen.availWidth", "innerWidth"],
    ["screen.availHeight", "innerHeight"],
    ["screen.left", 0],
    ["screen.top", 0],
    ["screen.availLeft", 0],
    ["screen.availTop", 0],
    ["screen.width", "innerWidth"],
    ["screen.height", "innerHeight"],
    ["screen.orientation.type", "'landscape-primary'"],
    ["screen.orientation.angle", 0],
    ["screen.mozOrientation", "'landscape-primary'"],
    ["devicePixelRatio", 1]
  ];

  // checkPair: tests if members of pair [a, b] are equal when evaluated.
  let checkPair = function (a, b) {
    is(eval(a), eval(b), a + " should be equal to " + b);
  };

  // Returns generator object that iterates through pref values.
  let prefVals = (for (prefVal of [false, true]) prefVal);

  // The main test function, runs until all pref values are exhausted.
  let nextTest = function () {
    let {value : prefValue, done} = prefVals.next();
    if (done) {
      SimpleTest.finish();
      return;
    }
    SpecialPowers.pushPrefEnv({set : [["privacy.resistFingerprinting", prefValue]]},
      function () {
        // We will be resisting fingerprinting if the pref is enabled,
        // and we are in a content script (not chrome).
        let resisting = prefValue && isContent;
        // Check each of the pairs.
        pairs.map(function ([item, onVal]) {
          if (resisting) {
            checkPair("window." + item, onVal);
          } else {
            if (!item.startsWith("moz")) {
              checkPair("window." + item, "chromeWindow." + item);
            }
          }
        });
        if (!resisting) {
          // Hard to predict these values, but we can enforce constraints:
          ok(window.mozInnerScreenX >= chromeWindow.mozInnerScreenX,
             "mozInnerScreenX");
          ok(window.mozInnerScreenY >= chromeWindow.mozInnerScreenY,
             "mozInnerScreenY");
        }
      nextTest();
    });
  }

  nextTest();
}
