function loop(f32, arr) {
    for (var i = 0; i < 2000; i++) {
        var j = i % 20;
        arr[j].k = f32[j];
    }
}

function f() {
    var obj = {k: null, m: null};
    var obj2 = {m: null, k: 42, l: null};
    var f32 = new Float32Array(20);
    var arr = [];
    for (var i = 0; i < 10; i++) {
        arr.push(obj);
        arr.push(obj2);

    }
    loop(f32, arr);
    for(var i = 0; i < 20; i++) {
        assertEq(arr[i].k, f32[i]);
    }
}

f();
