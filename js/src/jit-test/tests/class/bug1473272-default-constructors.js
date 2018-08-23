// Test the source location info in a derived-class default constructor.

function W() { test(); }
class Z extends W {}  // line 4
class Y extends Z {}  // line 5

class X extends Y {}  // line 7

function test() {
    for (let frame of new Error().stack.split('\n')) {
        function lineNumber(frame) {
            return +frame.match(/(\d+):\d+$/)[1];
        }

        if (frame.startsWith("Z@"))
            assertEq(lineNumber(frame), 4);
        if (frame.startsWith("Y@"))
            assertEq(lineNumber(frame), 5);
        if (frame.startsWith("X@"))
            assertEq(lineNumber(frame), 7);
    }
}

new X;
