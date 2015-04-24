var global = newGlobal();

var arrayIter = (new global.Array())[global.Symbol.iterator]();
var ArrayIteratorPrototype = Object.getPrototypeOf(arrayIter);
var arrayIterProtoBase = Object.getPrototypeOf(ArrayIteratorPrototype);
var IteratorPrototype = arrayIterProtoBase;
delete IteratorPrototype.next;

var obj = global.eval('({a: 1})')
for (var x in obj) {}
assertEq(x, "a");
