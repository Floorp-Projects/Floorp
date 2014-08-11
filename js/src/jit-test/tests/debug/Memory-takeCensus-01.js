// Debugger.Memory.prototype.takeCensus returns a value of an appropriate shape.

var dbg = new Debugger;

function checkProperties(census) {
  assertEq(typeof census, 'object');
  for (prop of Object.getOwnPropertyNames(census)) {
    var desc = Object.getOwnPropertyDescriptor(census, prop);
    assertEq(desc.enumerable, true);
    assertEq(desc.configurable, true);
    assertEq(desc.writable, true);
    if (typeof desc.value === 'object')
      checkProperties(desc.value);
    else
      assertEq(typeof desc.value, 'number');
  }
}

checkProperties(dbg.memory.takeCensus());

var g = newGlobal();
dbg.addDebuggee(g);
checkProperties(dbg.memory.takeCensus());
