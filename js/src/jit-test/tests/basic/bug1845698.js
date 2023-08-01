function f() {
    var o = {x: 1, y: 2};
    Object.defineProperty(o, "z", {value: 3, configurable: false});
    delete o.x;
    var snapshot = createShapeSnapshot(o);
    delete o.y;
    checkShapeSnapshot(snapshot);
}
f();
