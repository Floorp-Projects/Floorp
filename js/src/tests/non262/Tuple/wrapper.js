// |reftest| skip-if(!this.hasOwnProperty("Record"))

var boxO = Object(#[1, 2, 3]);

assertEq(Object.isExtensible(boxO), false);
assertEq(Object.isSealed(boxO), true);
assertEq(Object.isFrozen(boxO), true);

boxO[0] = 3;
assertEq(boxO[0], 1);
assertThrowsInstanceOf(() => { "use strict"; boxO[0] = 3; }, TypeError);
assertEq(boxO[0], 1);

boxO[4] = 3;
assertEq(boxO[4], undefined);
assertThrowsInstanceOf(() => { "use strict"; boxO[4] = 3; }, TypeError);
assertEq(boxO[4], undefined);

assertThrowsInstanceOf(() => { Object.defineProperty(boxO, "0", { value: 3 }); }, TypeError);
assertEq(boxO[0], 1);

assertThrowsInstanceOf(() => { Object.defineProperty(boxO, "4", { value: 3 }); }, TypeError);
assertEq(boxO[4], undefined);

if (typeof reportCompare === "function") reportCompare(0, 0);
