function mapped() {
    var Iterator = {};

    // First overwrite the iterator.
    arguments[Symbol.iterator] = Iterator;

    // And then redefine a property attribute.
    Object.defineProperty(arguments, Symbol.iterator, {
        writable: false
    });

    // Make sure redefining an attribute doesn't reset the iterator value.
    assertEq(arguments[Symbol.iterator], Iterator);
}
mapped();

function unmapped() {
  "use strict";

  var Iterator = {};

  // First overwrite the iterator.
  arguments[Symbol.iterator] = Iterator;

  // And then redefine a property attribute.
  Object.defineProperty(arguments, Symbol.iterator, {
      writable: false
  });

  // Make sure redefining an attribute doesn't reset the iterator value.
  assertEq(arguments[Symbol.iterator], Iterator);
}
unmapped();
