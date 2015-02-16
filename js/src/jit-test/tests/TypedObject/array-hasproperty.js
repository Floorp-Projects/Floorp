if (!this.hasOwnProperty("TypedObject"))
  quit();

var array = new (TypedObject.uint8.array(5));

for (var i = 0; i < array.length; i++)
    assertEq(i in array, true);

for (var v of [20, 300, -1, 5, -10, Math.pow(2, 32) - 1, -Math.pow(2, 32)])
    assertEq(v in array, false);

// Don't inherit elements
array.__proto__[50] = "hello";
assertEq(array.__proto__[50], "hello");
assertEq(50 in array, false);

// Do inherit normal properties
array.__proto__.a = "world";
assertEq(array.__proto__.a, "world");
assertEq("a" in array, true);
