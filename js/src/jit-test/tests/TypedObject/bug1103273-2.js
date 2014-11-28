if (!this.hasOwnProperty("TypedObject"))
  quit();

var Vec3u16Type = TypedObject.uint16.array(3);

function foo() {
    var x = 0;
    for (var i = 0; i < 3; i++) {
        var obj = new Vec3u16Type;
        var buf = TypedObject.storage(obj).buffer;
        var arr = new Uint8Array(buf, 3);
        arr[0] = i + 1;
        arr[1] = i + 2;
        arr[2] = i + 3;
        for (var j = 0; j < arr.length; j++) {
            minorgc();
            x += arr[j];
        }
    }
    assertEq(x, 27);
}
foo();
