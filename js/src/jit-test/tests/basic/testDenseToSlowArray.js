// test dense -> slow array transitions during the recording and on trace
// for various array functions and property accessors

function test_set_elem() {

    function f() {
        var bag = [];
        for (var i = 0; i != 100; ++i) {
            var a = [0];
            a[100*100] = i;
            bag.push(a);
        }
        
        for (var i = 0; i != 100; ++i) {
            var a = [0];
            a[200 + i] = i;
            bag.push(a);
        }
        return bag;
    }
    
    var bag = f();
    
    for (var i = 0; i != 100; ++i) {
        var a = bag[i];
        assertEq(a.length, 100 * 100 + 1);
        assertEq(a[100*100], i);
        assertEq(a[0], 0);
        assertEq(1 + i in a, false);
    }
    
    for (var i = 0; i != 100; ++i) {
        var a = bag[100 + i];
        assertEq(a.length, 200 + i + 1);
        assertEq(a[200 + i], i);
        assertEq(a[0], 0);
        assertEq(1 + i in a, false);
    }
}

function test_reverse() {

    function prepare_arays() {
        var bag = [];
        var base_index = 245;
        for (var i = 0; i != 50; ++i) {
            var a = [1, 2, 3, 4, 5];
            a.length = i + base_index;
            bag.push(a);
        }
        return bag;
    }

    function test(bag) {
        for (var i = 0; i != bag.length; ++i) {
            var a = bag[i];
            a.reverse();
            a[0] = 1;
        }
    }

    var bag = prepare_arays();
    test(bag);
    for (var i = 0; i != bag.length; ++i) {
        var a = bag[i];
        assertEq(a[0], 1);
        for (var j = 1; j <= 5; ++j) {
            assertEq(a[a.length - j], j);
        }
        for (var j = 1; j < a.length - 5; ++j) {
            assertEq(j in a, false);
        }
    }
    
}

function test_push() {

    function prepare_arays() {
        var bag = [];
        var base_index = 245;
        for (var i = 0; i != 50; ++i) {
            var a = [0];
            a.length = i + base_index;
            bag.push(a);
        }
        return bag;
    }

    function test(bag) {
        for (var i = 0; i != bag.length; ++i) {
            var a = bag[i];
            a.push(2);
            a[0] = 1;
        }
    }

    var bag = prepare_arays();
    test(bag);
    for (var i = 0; i != bag.length; ++i) {
        var a = bag[i];
        assertEq(a[0], 1); 
        assertEq(a[a.length - 1], 2);
        for (var j = 1; j < a.length - 1; ++j) {
            assertEq(j in a, false);
        }
    }
}

function test_unshift() {

    function prepare_arays() {
        var bag = [];
        var base_index = 245;
        for (var i = 0; i != 50; ++i) {
            var a = [0];
            a.length = i + base_index;
            bag.push(a);
        }
        return bag;
    }

    function test(bag) {
        for (var i = 0; i != bag.length; ++i) {
            var a = bag[i];
            a.unshift(1);
            a[2] = 2;
        }
    }

    var bag = prepare_arays();
    test(bag);
    for (var i = 0; i != bag.length; ++i) {
        var a = bag[i];
        assertEq(a[0], 1); 
        assertEq(a[1], 0); 
        assertEq(a[2], 2); 
        for (var j = 3; j < a.length; ++j) {
            assertEq(j in a, false);
        }
    }
}

function test_splice() {

    function prepare_arays() {
        var bag = [];
        var base_index = 245;
        for (var i = 0; i != 50; ++i) {
            var a = [1, 2];
            a.length = i + base_index;
            bag.push(a);
        }
        return bag;
    }

    function test(bag) {
        for (var i = 0; i != bag.length; ++i) {
            var a = bag[i];
            a.splice(1, 0, "a", "b", "c");
            a[2] = 100;
        }
    }

    var bag = prepare_arays();
    test(bag);
    for (var i = 0; i != bag.length; ++i) {
        var a = bag[i];
        assertEq(a[0], 1); 
        assertEq(a[1], "a"); 
        assertEq(a[2], 100); 
        assertEq(a[3], "c"); 
        assertEq(a[4], 2); 
        for (var j = 5; j < a.length; ++j) {
            assertEq(j in a, false);
        }
    }
}

test_set_elem();
test_reverse();
test_push();
test_unshift();
test_splice();

