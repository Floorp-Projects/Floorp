
if (typeof TypedObject === "undefined")
    quit();

// Test the relationships between prototypes for array typed objects.

var arrA = new TypedObject.ArrayType(TypedObject.int32, 10);
var arrB = new TypedObject.ArrayType(TypedObject.int32, 20);
var arrC = new TypedObject.ArrayType(TypedObject.int8, 10);

assertEq(arrA.prototype == arrB.prototype, true);
assertEq(arrA.prototype == arrC.prototype, false);
assertEq(Object.getPrototypeOf(arrA.prototype) == Object.getPrototypeOf(arrC.prototype), true);
assertEq(Object.getPrototypeOf(arrA.prototype) == TypedObject.ArrayType.prototype.prototype, true);

var instanceA = new arrA();
var instanceB = new arrB();
var instanceC = new arrC();

assertEq(Object.getPrototypeOf(instanceA) == arrA.prototype, true);
assertEq(Object.getPrototypeOf(instanceB) == arrB.prototype, true);
assertEq(Object.getPrototypeOf(instanceC) == arrC.prototype, true);
