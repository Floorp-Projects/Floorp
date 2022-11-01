// Test for OrdinaryToPrimitive called on a plain object with hint "string".

function assertStr(o, s) {
  assertEq(String(o), s);
}
function test() {
  assertStr({x: 1}, "[object Object]");
  assertStr({[Symbol.toStringTag]: "Foo"}, "[object Foo]");
  assertStr({toString() { return 123; }}, "123");
  assertStr({toString: Math.abs}, "NaN");
  assertStr({x: "hello", toString() { return this.x; }}, "hello");

  let c = 0;
  let fun = () => "hi-" + ++c;
  assertStr({toString: fun}, "hi-1");
  assertStr({toString: "foo", valueOf: fun}, "hi-2");
  assertStr({toString() { return {}; }, valueOf: fun}, "hi-3");

  let proto = {};
  proto[Symbol.toStringTag] = null;
  assertStr(Object.create(proto), "[object Object]");
  proto[Symbol.toStringTag] = "Bar";
  assertStr(Object.create(proto), "[object Bar]");
}
test();
