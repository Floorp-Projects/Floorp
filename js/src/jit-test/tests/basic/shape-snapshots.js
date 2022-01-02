function test1() {
    var o = {x: 1, y: 2};
    var snapshot = createShapeSnapshot(o);
    checkShapeSnapshot(snapshot);
    Object.defineProperty(o, "z", {get: function() {}});
    checkShapeSnapshot(snapshot);

    snapshot = createShapeSnapshot(o);
    checkShapeSnapshot(snapshot);
    o[12345678] = 1;
    checkShapeSnapshot(snapshot);

    snapshot = createShapeSnapshot(o);
    Object.defineProperty(o, "a", {configurable: true, set: function(){}});
    checkShapeSnapshot(snapshot);

    snapshot = createShapeSnapshot(o);
    checkShapeSnapshot(snapshot);
    delete o.a;
    checkShapeSnapshot(snapshot);
}
test1();

function test2() {
    var dictObject = {x: 1, y: 2, z: 3};
    delete dictObject.x;
    var objects = [this, {}, {x: 1}, {x: 2}, dictObject, function() {}, [1, 2],
                   Object.prototype, new Proxy({}, {})];
    var snapshots = objects.map(o => createShapeSnapshot(o));
    gc();
    snapshots.forEach(function(snapshot) {
        objects.forEach(function(obj) {
            checkShapeSnapshot(snapshot, obj);
        });
    });
}
test2();
