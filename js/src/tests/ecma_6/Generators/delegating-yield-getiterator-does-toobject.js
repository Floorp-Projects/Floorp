// With yield*, the GetIterator call on the provided value looks up the
// @@iterator property on ToObject(value), not on a possibly-primitive value.
"use strict";

Object.defineProperty(Boolean.prototype, Symbol.iterator,
{
  get() {
    assertEq(typeof this, "object",
             "GetMethod(obj, @@iterator) internally first performs ToObject");
    assertEq(this !== null, true,
             "this is really an object, not null");

    function FakeBooleanIterator()
    {
      assertEq(typeof this, "boolean",
               "iterator creation code must perform ToObject(false) before " +
               "doing a lookup for @@iterator");

      var count = 0;
      return { next() {
                 if (count++ > 0)
                   throw new Error("unexpectedly called twice");
                 return { done: true };
               } };

    }

    return FakeBooleanIterator;
  },
  configurable: true
});

function* f()
{
  yield 1;
  yield* false;
  yield 2;
}

for (var i = 0; i < 10; i++)
{
  var gen = f();

  var first = gen.next();
  assertEq(first.done, false);
  assertEq(first.value, 1);

  var second = gen.next();
  assertEq(second.done, false);
  assertEq(second.value, 2);

  var last = gen.next();
  assertEq(last.done, true);
  assertEq(last.value, undefined);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
