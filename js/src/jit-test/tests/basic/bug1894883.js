function makeObj() { return { x: 1, y: 2 }; }

var o1 = makeObj();
var o2 = makeObj();

var snapshot = createShapeSnapshot(o1);
delete o1.x;
checkShapeSnapshot(snapshot, o2);
