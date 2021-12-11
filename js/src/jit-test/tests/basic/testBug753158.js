function testFuncStmt1() {
    var g = 3;
    function f(b) {
        if (b) {
            function g() { return 42 }
            assertEq(g(), 42);
        }
    }
    f(true);
}
testFuncStmt1();

function testFuncStmt2() {
    var g = 3;
    (function(b) {
        if (b) {
            function g() { return 42 }
            function f() { assertEq(g(), 42); }
            f();
        }
    })(true);
}
testFuncStmt2();

function testEval1() {
    var g = 3;
    function f() {
        eval("var g = 42");
        assertEq(g, 42);
    }
    f();
}
testEval1();

function testEval2() {
    var g = 3;
    (function() {
        eval("var g = 42");
        function f() {
            assertEq(g, 42);
        }
        f();
    })();
}
testEval2();

function testWith1() {
    var g = 3;
    function f() {
        with ({g:42}) {
            assertEq(g, 42);
        }
    }
    f();
}
testWith1();

function testWith2() {
    var g = 3;
    with ({g:42}) {
        function f() {
            assertEq(g, 42);
        }
    }
    f();
}
testWith2();
