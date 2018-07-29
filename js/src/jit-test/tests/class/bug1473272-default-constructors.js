// Test the source location info in a derived-class default constructor.

function W() { test(); }
class Z extends W {}  // line 4
class Y extends Z {}  // line 5

class X extends Y {}  // line 7

function test() {
    for (let line of new Error().stack.split('\n')) {
        if (line.startsWith("Z@"))
            assertEq(line.split(":")[1], "4");
        if (line.startsWith("Y@"))
            assertEq(line.split(":")[1], "5");
        if (line.startsWith("X@"))
            assertEq(line.split(":")[1], "7");
    }
}

new X;
