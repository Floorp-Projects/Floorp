// Same thing but nested trees, each reconstructing one or more stack frames 
// (so, several functions with loops, such that the loops end up being
// nested though they are not lexically nested)

function testSlowArrayPopNestedTrees() {    
    var a = [];
    for (var i = 0; i < 9; i++)
        a[i] = [0];
    a[8].__defineGetter__("0", function () { return 3.14159 });

    function child(a, i, j, k) {
        var last = 2.71828;
        for (var l = 0; l < 9; l++)
            if (i == 8 && j == 8 && k == 8)
                last = a[l].pop();  // reenters interpreter in getter
        return last;
    }
    function parent(a, i, j) {
        var last;
        for (var k = 0; k < 9; k++)
            last = child(a, i, j, k);
        return last;
    }
    function gramps(a, i) { 
        var last;
        for (var j = 0; j < 9; j++)
            last = parent(a, i, j);
        return last;
    }

    var last;
    for (var i = 0; i < 9; i++)
        last = gramps(a, i);
    return last;
}
assertEq(testSlowArrayPopNestedTrees(), 3.14159);
