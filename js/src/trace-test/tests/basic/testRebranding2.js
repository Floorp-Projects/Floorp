delete q;
delete g;
delete h;
delete a;
delete f;

function testRebranding2() {
    // Same as testRebranding, but the object to be rebranded isn't the global.
    var x = "FAIL";
    function g(){}
    function h(){ x = "ok"; }
    var obj = {m: g};
    var arr = [g, g, g, g, h];
    assertEq(arr.length > RUNLOOP, true);
    for (var i = 0; i < 5; i++) {
        obj.m = arr[i];
        obj.m();
    }
    return x;
}
assertEq(testRebranding2(), "ok");
