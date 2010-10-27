// Same thing but nested trees, each reconstructing one or more stack frames 
// (so, several functions with loops, such that the loops end up being
// nested though they are not lexically nested)

function testSlowArrayPopNestedTrees() {    
    var a = [];
    for (var i = 0; i < RUNLOOP; i++)
        a[i] = [0];
    a[RUNLOOP-1].__defineGetter__("0", function () { return 3.14159 });

    function child(a, i, j, k) {
        var last = 2.71828;
        for (var l = 0; l < RUNLOOP; l++)
            if (i == RUNLOOP-1 && j == RUNLOOP-1 && k == RUNLOOP-1)
                last = a[l].pop();  // reenters interpreter in getter
        return last;
    }
    function parent(a, i, j) {
        var last;
        for (var k = 0; k < RUNLOOP; k++)
            last = child(a, i, j, k);
        return last;
    }
    function gramps(a, i) { 
        var last;
        for (var j = 0; j < RUNLOOP; j++)
            last = parent(a, i, j);
        return last;
    }

    var last;
    for (var i = 0; i < RUNLOOP; i++)
        last = gramps(a, i);
    return last;
}
assertEq(testSlowArrayPopNestedTrees(), 3.14159);
