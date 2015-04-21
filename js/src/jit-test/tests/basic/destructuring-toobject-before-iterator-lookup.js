// Test that looking up a supposedly-iterable value's @@iterator property
// first boxes up the value into an object via ToObject, then gets the
// @@iterator using the object as receiver.  Cover most of the cases where the
// frontend effects a GetIterator call.  (The remaining frontend cases are
// handled in other tests in the revision that introduced this file.
// Non-frontend cases aren't tested here.)
"use strict";

load(libdir + 'asserts.js');
load(libdir + 'eqArrayHelper.js');

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

function destructuringAssignment()
{
  ([] = false);
  ([,] = false);
}
for (var i = 0; i < 10; i++)
  destructuringAssignment();

function spreadElements()
{
  var arr1 = [...false];
  assertEqArray(arr1, []);

  var arr2 = [1, ...false];
  assertEqArray(arr2, [1]);

  var arr3 = [1, ...false, 2];
  assertEqArray(arr3, [1, 2]);
}
for (var i = 0; i < 10; i++)
  spreadElements();

function spreadCall()
{
  var arr1 = new Array(...false);
  assertEqArray(arr1, []);

  var arr2 = new Array(0, ...false);
  assertEqArray(arr2, []);

  var arr3 = new Array(1, 2, ...false);
  assertEqArray(arr3, [1, 2]);
}
for (var i = 0; i < 10; i++)
  spreadCall();

function destructuringArgument([])
{
}
for (var i = 0; i < 10; i++)
  destructuringArgument(false);
