var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);
var gw = dbg.addDebuggee(g);

g.eval(`
  var obj = {
    p: 0,
    [Symbol.iterator]: 0,
  };
`);

// Return |key| as an object.
function toObject(key) {
  return {
    [Symbol.toPrimitive]() {
      return key;
    }
  };
}

let obj = gw.getProperty("obj").return;

for (let key of obj.getOwnPropertyNames()) {
  let keyObject = toObject(key);

  g.obj[key] = 1;
  assertEq(g.obj[key], 1);
  assertEq(obj.deleteProperty(key), true);
  assertEq(g.obj[key], undefined);

  g.obj[key] = 1;
  assertEq(g.obj[key], 1);
  assertEq(obj.deleteProperty(keyObject), true);
  assertEq(g.obj[key], undefined);
}

for (let key of obj.getOwnPropertySymbols()) {
  let keyObject = toObject(key);

  g.obj[key] = 1;
  assertEq(g.obj[key], 1);
  assertEq(obj.deleteProperty(key), true);
  assertEq(g.obj[key], undefined);

  g.obj[key] = 1;
  assertEq(g.obj[key], 1);
  assertEq(obj.deleteProperty(keyObject), true);
  assertEq(g.obj[key], undefined);
}
