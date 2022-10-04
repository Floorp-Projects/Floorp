// |reftest| shell-option(--enable-shadow-realms) skip-if(!xulRuntime.shell)

let sr = new ShadowRealm();
var f = sr.evaluate("function f() { }; f");

assertEq(/native code/.test(f.toString()), true)
assertEq(/function f/.test(f.toString()), true)

f.name = "koala"

assertEq(/function koala/.test(f.toString()), false)

Object.defineProperty(f, "name", { writable: true, value: "koala" });
assertEq(/function koala/.test(f.toString()), true)

f.name = "panda"
assertEq(/function panda/.test(f.toString()), true)


if (typeof reportCompare === 'function')
    reportCompare(true, true);
