var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);
var gw = dbg.addDebuggee(g);

g.eval(`
  var obj = {
    p: 1,
    [Symbol.iterator]: 2,
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
  let value = g.obj[key];

  assertEq(obj.getProperty(key).return, value);
  assertEq(obj.getProperty(keyObject).return, value);
}

for (let key of obj.getOwnPropertySymbols()) {
  let keyObject = toObject(key);
  let value = g.obj[key];

  assertEq(obj.getProperty(key).return, value);
  assertEq(obj.getProperty(keyObject).return, value);
}
