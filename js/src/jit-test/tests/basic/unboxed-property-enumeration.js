function O() {
    this.x = 1;
    this.y = 2;
}
function testUnboxed() {
    var arr = [];
    for (var i=0; i<100; i++)
	arr.push(new O);

    var o = arr[arr.length-1];
    o[0] = 0;
    o[2] = 2;
    var sym = Symbol();
    o[sym] = 1;
    o.z = 3;
    Object.defineProperty(o, '3', {value:1,enumerable:false,configurable:false,writable:false});
    o[4] = 4;

    var props = Reflect.ownKeys(o);
    assertEq(props[props.length-1], sym);

    // Note: this should really be 0234xyz, this is bug 1175111. It has been
    // fixed on newer branches.
    assertEq(Object.getOwnPropertyNames(o).join(""), "024xyz3");
}
testUnboxed();
