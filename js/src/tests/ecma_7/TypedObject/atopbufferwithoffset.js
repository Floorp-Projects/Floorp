// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 898356;

var {StructType, uint32, Object, Any, storage, objectType} = TypedObject;

function main() { // once a C programmer, always a C programmer.
  print(BUGNUMBER + ": " + summary);

  var Uints = new StructType({f: uint32, g: uint32});

  var anArray = new Uint32Array(4);
  anArray[1] = 22;
  anArray[2] = 44;

  var uints = new Uints(anArray.buffer, 4);
  assertEq(storage(uints).buffer, anArray.buffer);
  assertEq(uints.f, 22);
  assertEq(uints.g, 44);
  uints.f++;
  assertEq(anArray[1], 23);

  // No misaligned byte offsets or offsets that would stretch past the end:
  assertThrows(() => new Uints(anArray.buffer, -4)); // negative
  assertThrows(() => new Uints(anArray.buffer, -3)); // negative
  assertThrows(() => new Uints(anArray.buffer, -2)); // negative
  assertThrows(() => new Uints(anArray.buffer, -1)); // negative
  new Uints(anArray.buffer, 0);                      // ok
  assertThrows(() => new Uints(anArray.buffer, 1));  // misaligned
  assertThrows(() => new Uints(anArray.buffer, 2));  // misaligned
  assertThrows(() => new Uints(anArray.buffer, 3));  // misaligned
  new Uints(anArray.buffer, 4);                      // ok
  assertThrows(() => new Uints(anArray.buffer, 5));  // misaligned
  assertThrows(() => new Uints(anArray.buffer, 6));  // misaligned
  assertThrows(() => new Uints(anArray.buffer, 7));  // misaligned
  new Uints(anArray.buffer, 8);                      // ok
  assertThrows(() => new Uints(anArray.buffer, 9));  // misaligned
  assertThrows(() => new Uints(anArray.buffer, 10)); // misaligned
  assertThrows(() => new Uints(anArray.buffer, 11)); // misaligned
  assertThrows(() => new Uints(anArray.buffer, 12)); // would extend past end
  assertThrows(() => new Uints(anArray.buffer, 13)); // misaligned
  assertThrows(() => new Uints(anArray.buffer, 14)); // misaligned
  assertThrows(() => new Uints(anArray.buffer, 15)); // misaligned
  assertThrows(() => new Uints(anArray.buffer, 16)); // would extend past end
  assertThrows(() => new Uints(anArray.buffer, 17)); // misaligned
  assertThrows(() => new Uints(anArray.buffer, 4294967292)); // overflows int

  reportCompare(true, true);
  print("Tests complete");
}

main();
