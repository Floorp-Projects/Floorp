var g = newGlobal({newCompartment: true});
var dbg = new Debugger;
var gDO = dbg.addDebuggee(g);

function check(expr, expected) {
  let completion = gDO.executeInGlobal(expr);
  assertEq(!completion.throw, true);

  let func = completion.return;
  assertEq(func.displayName, expected);
}

check("(function foo(){})", "foo");
check("Map.prototype.set", "set");
check("Object.getOwnPropertyDescriptor(Map.prototype, 'size').get", "get size");
check("Object.getOwnPropertyDescriptor(RegExp.prototype, 'flags').get", "get flags");
