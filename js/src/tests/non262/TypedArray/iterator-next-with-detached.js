function checkResult(actual, expected)
{
  assertEq(actual.value, expected.value);
  assertEq(actual.done, expected.done);
}

if (typeof detachArrayBuffer === "function" && typeof newGlobal === "function")
{
  var iteratorFunction = Uint8Array.prototype[Symbol.iterator];


  var thisGlobal = this;
  var otherGlobal = newGlobal();

  var thisNext = new Uint8Array()[Symbol.iterator]().next

  for (const constructor of typedArrayConstructors)
  {
    assertEq(new constructor()[Symbol.iterator]().next, thisNext);

    var globals =
      [
       [thisGlobal, thisGlobal],
       [thisGlobal, otherGlobal],
       [otherGlobal, otherGlobal],
       [otherGlobal, thisGlobal],
      ];

    for (const [arrayGlobal, bufferGlobal] of globals)
    {
      var arr, buffer, iterator;

      function arrayBufferIterator()
      {
        var byteLength = 2 * constructor.BYTES_PER_ELEMENT;
        var buf = new bufferGlobal.ArrayBuffer(byteLength);
        var tarray = new arrayGlobal[constructor.name](buf);

        tarray[0] = 1;
        tarray[1] = 2;

        return [tarray, buf, Reflect.apply(iteratorFunction, tarray, [])];
      }

      [arr, buffer, iterator] = arrayBufferIterator();
      checkResult(thisNext.call(iterator), {value: 1, done: false});
      checkResult(thisNext.call(iterator), {value: 2, done: false});
      checkResult(thisNext.call(iterator), {value: undefined, done: true});

      // Test an exhausted iterator.
      bufferGlobal.detachArrayBuffer(buffer);
      checkResult(thisNext.call(iterator), {value: undefined, done: true});

      // Test an all-but-exhausted iterator.
      [arr, buffer, iterator] = arrayBufferIterator();
      checkResult(thisNext.call(iterator), {value: 1, done: false});
      checkResult(thisNext.call(iterator), {value: 2, done: false});

      bufferGlobal.detachArrayBuffer(buffer);
      assertThrowsInstanceOf(() => thisNext.call(iterator), TypeError);

      // Test an unexhausted iterator.
      [arr, buffer, iterator] = arrayBufferIterator();
      checkResult(thisNext.call(iterator), {value: 1, done: false});

      bufferGlobal.detachArrayBuffer(buffer);
      assertThrowsInstanceOf(() => thisNext.call(iterator), TypeError);
    }
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
