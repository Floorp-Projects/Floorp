function testSmallIndex() {
   var proto = Object.create(null);
   var arr = [];
   Object.setPrototypeOf(arr, proto);

   proto[0] = 123;
   Object.freeze(proto);

   for (var i = 0; i < 20; i++) {
      arr[0] = 321;
   }
   assertEq(arr[0], 123);
}
testSmallIndex();

function testLargeIndex() {
   var proto = Object.create(null);
   var arr = [];
   Object.setPrototypeOf(arr, proto);

   proto[98765432] = 123;
   Object.freeze(proto);

   for (var i = 0; i < 20; i++) {
      arr[98765432] = 321;
   }
   assertEq(arr[98765432], 123);
}
testLargeIndex();
