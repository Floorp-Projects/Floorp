for (let ctor of typedArrayConstructors) {
  let arr = new ctor([1, 2, 3, 4, 5, 6, 7, 8]);

  arr.buffer.constructor = {
    get [Symbol.species]() {
      throw new Error("unexpected @@species access");
    }
  };

  for (let ctor2 of typedArrayConstructors) {
    let arr2 = new ctor2(arr);

    assertEq(Object.getPrototypeOf(arr2.buffer), ArrayBuffer.prototype);
    assertEq(arr2.buffer.constructor, ArrayBuffer);
  }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
