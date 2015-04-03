// Tests that exception handling works with block scopes.

var g = newGlobal();
var dbg = new Debugger(g);
var correct;
dbg.onEnterFrame = function (f) {
  if (f.callee && f.callee.name == "f") {
    f.onPop = function() {
      // The scope at the point of onPop is at the point of popping (the
      // noSuchFn call).
      correct = (f.environment.getVariable("e") === 42 &&
                 f.environment.getVariable("outer") === undefined);
    };
  }
};
g.eval("" + function f() {
  var outer = 43;
  // Surround with a loop to insert a loop trynote.
  for (;;) {
    try {
      eval("");
      throw 42;
    } catch (e) {
      noSuchFn(e);
    }
  }
});


try {
  g.eval("f();");
} catch (e) {
  // The above is expected to throw.
}

assertEq(correct, true);
