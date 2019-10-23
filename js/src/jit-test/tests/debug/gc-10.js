// Debugger.Frame objects should not be GC'd when doing so would have observable
// effects.

var g = newGlobal({ newCompartment: true });

var log = '';
var saved;

new Debugger(g).onDebuggerStatement = function (frame) {

  // Having a live onDebuggerStatement hook will (correctly) cause a Debugger to
  // be retained, even if it is otherwise unreachable.
  this.onDebuggerStatement = undefined;

  // Give this Debugger.Frame an observable effect. It should not be GC'd.
  frame.onPop = function () {
    log += 'p';
  }
}

g.parent = this;

g.eval(`
  debugger;
  gc();
`);

assertEq(log, 'p');
