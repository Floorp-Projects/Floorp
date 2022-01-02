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

  obj.defineProperty(key, {value: 1});
  assertEq(g.obj[key], 1);

  obj.defineProperty(keyObject, {value: 1});
  assertEq(g.obj[key], 1);
}

for (let key of obj.getOwnPropertySymbols()) {
  let keyObject = toObject(key);

  obj.defineProperty(key, {value: 1});
  assertEq(g.obj[key], 1);

  obj.defineProperty(keyObject, {value: 1});
  assertEq(g.obj[key], 1);
}
