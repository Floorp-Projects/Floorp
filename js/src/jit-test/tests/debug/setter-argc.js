// Check that setters throw TypeError when passed no arguments, instead of crashing.

function check(obj) {
  let proto = Object.getPrototypeOf(obj);
  let props = Object.getOwnPropertyNames(proto);
  for (let prop of props) {
    let desc = Object.getOwnPropertyDescriptor(proto, prop);
    if (desc.set) {
      print("bleah: " + uneval(prop));
      assertEq(typeof desc.set, 'function');
      try {
        desc.set.call(obj);
        assertEq("should have thrown TypeError", false);
      } catch (e) {
        assertEq(e instanceof TypeError, true);
      }
    }
  }
}

var dbg = new Debugger;
var g = newGlobal();
var gw = dbg.addDebuggee(g);

// Debugger
check(dbg);

// Debugger.Memory
check(dbg.memory);

// Debugger.Object
g.eval('function f() { debugger; }');
var fw = gw.getOwnPropertyDescriptor('f').value;
check(fw);

// Debugger.Script
check(fw.script);

// Debugger.Source
check(fw.script.source);

// Debugger.Environment
check(fw.environment);

// Debugger.Frame
var log = '';
dbg.onDebuggerStatement = function(frame) {
  log += 'd';
  check(frame);
}
g.eval('f()');
assertEq(log, 'd');
