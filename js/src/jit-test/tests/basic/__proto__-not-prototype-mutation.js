/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var __proto__ = "string value";

var propdefs =
  [{ c: "['__proto__']: null", t: "object" },
   { c: "__proto__() {}", t: "function" },
   { c: "['__proto__']() {}", t: "function" },
   { c: "*__proto__() {}", t: "function" },
   { c: "*['__proto__']() {}", t: "function" },
   { c: "__proto__", t: "string" },
   { c: "get __proto__() { return 42; }", t: "number" },
   { c: "get ['__proto__']() { return undefined; }", t: "undefined" },
   { c: "set __proto__(v) { }", t: "undefined" },
   { c: "set ['__proto__'](v) { }", t: "undefined" }];

for (var propdef of propdefs)
{
  var f = new Function("return { " + propdef.c + " };");
  for (var i = 0; i < 100; i++)
  {
    var inst = f();
    assertEq(typeof inst.__proto__, propdef.t);
    assertEq(Object.getPrototypeOf(inst), Object.prototype);
  }
}
