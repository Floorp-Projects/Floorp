/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

var someObject = {
  a: 1,
  func: function()
  {
    this.b = 2;
  }
};

var anotherObject = {
  _finalize: function()
  {
    someObject.c = 3;
  }
};

function test() {
  ok(TiltUtils, "The TiltUtils object doesn't exist.");

  TiltUtils.bindObjectFunc(someObject, "", anotherObject);
  someObject.func();

  is(someObject.a, 1,
    "The bindObjectFunc() messed the non-function members of the object.");
  isnot(someObject.b, 2,
    "The bindObjectFunc() didn't ignore the old scope correctly.");
  is(anotherObject.b, 2,
    "The bindObjectFunc() didn't set the new scope correctly.");


  TiltUtils.destroyObject(anotherObject);
  is(someObject.c, 3,
    "The finalize function wasn't called when an object was destroyed.");


  TiltUtils.destroyObject(someObject);
  is(typeof someObject.a, "undefined",
    "Not all members of the destroyed object were deleted.");
  is(typeof someObject.func, "undefined",
    "Not all function members of the destroyed object were deleted.");
}
