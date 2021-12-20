// |reftest| skip-if(!this.hasOwnProperty("Record"))

var boxO = Object(#{ x: 1, y: 2 });

assertEq(Object.isExtensible(boxO), false);
assertEq(Object.isSealed(boxO), true);
assertEq(Object.isFrozen(boxO), true);

boxO.x = 3;
assertEq(boxO.x, 1);
assertThrowsInstanceOf(() => { "use strict"; boxO.x = 3; }, TypeError);
assertEq(boxO.x, 1);

boxO.z = 3;
assertEq(boxO.z, undefined);
assertThrowsInstanceOf(() => { "use strict"; boxO.z = 3; }, TypeError);
assertEq(boxO.z, undefined);

assertThrowsInstanceOf(() => { Object.defineProperty(boxO, "x", { value: 3 }); }, TypeError);
assertEq(boxO.x, 1);

assertThrowsInstanceOf(() => { Object.defineProperty(boxO, "z", { value: 3 }); }, TypeError);
assertEq(boxO.z, undefined);

if (typeof reportCompare === "function") reportCompare(0, 0);
