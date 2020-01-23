var g = newGlobal({sameCompartmentAs: this});
var newTarget = g.Function();
newTarget.prototype = undefined;

// Reflect.construct returns an object in the current realm
// but its prototype is g's Array.prototype.
var arr1 = Reflect.construct(Array, [], newTarget);
assertEq(objectGlobal(arr1), this);
assertEq(arr1.__proto__, g.Array.prototype);

// Calling cross-realm slice creates an object in that realm.
for (var i = 0; i < 10; i++) {
    var arr2 = arr1.slice();
    assertEq(objectGlobal(arr2), g);
}
