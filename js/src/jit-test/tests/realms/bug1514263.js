function O(x) {
    this.x = x;
}

var arr = [];
for (var i = 0; i < 100; i++) {
    arr.push(new O(i));
}

var g = newGlobal({sameCompartmentAs: this});
g.trigger = function(arr) {
    var obj = arr[90];
    this.Object.create(obj);
    assertEq(objectGlobal(obj), objectGlobal(arr));
};
g.trigger(arr);
