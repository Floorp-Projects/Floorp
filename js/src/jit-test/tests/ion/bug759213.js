// |jit-test| error: ReferenceError
function testNestedForIn() {
    var a = {x: 1, y: (/\\u00fd[]/ ), z: 3};
    for (var p1 in a)
        for (var { w  }  = 0     ;  ;    )
            testJSON(t);
}

assertEq(testNestedForIn(), 'xx xy xz yx yy yz zx zy zz ');
