var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);

g.eval(`
function f() {
  // |this| in arrow-functions refers to the |this| binding in outer functions.
  // So when |frame.eval("this")| is executed, the outer |this| binding should
  // be returned, unless it has been optimised out.
  (() => {})();

  // Ensure a |this| binding is created for |f|.
  return this;
}
`);

var errors = [];

function enterFrame(frame) {
  // Disable the handler so we don't call it recursively through |frame.eval|.
  dbg.onEnterFrame = undefined;

  // Store the error when resolving |this| was unsuccessful.
  var r = frame.eval("this");
  if (r.throw) {
    errors.push(r.throw);
  }

  // Re-enable the handler.
  dbg.onEnterFrame = enterFrame;
};

dbg.onEnterFrame = enterFrame;

g.f();

assertEq(errors.length, 1);
assertEq(errors[0].unsafeDereference().toString(),
         "Error: variable 'this' has been optimized out");
