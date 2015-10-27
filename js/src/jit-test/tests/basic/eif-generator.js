load(libdir + "evalInFrame.js");

function f() {
    {
        let x = 1;
        while (true) {
            yield evalInFrame(0, "x");
            x++;
            {
                let y = 1;
                yield evalInFrame(0, "++y");
                yield evalInFrame(0, "++y");
            }
        }
    }
}

var gen = f();
assertEq(gen.next(), 1);
assertEq(gen.next(), 2);
gc();
assertEq(gen.next(), 3);
gc();
assertEq(gen.next(), 2);
assertEq(gen.next(), 2);
gc();
assertEq(gen.next(), 3);
gc();
assertEq(gen.next(), 3);
assertEq(gen.next(), 2);
gc();
assertEq(gen.next(), 3);
gen = null;
gc();

function g() {
    {
        let x = 1;
        while (true) {
            var inner = function (inc) { x += inc; return evalInFrame(0, "x") };
            assertEq(inner(0), x);
            yield inner;
            assertEq(inner(0), x);
        }
    }
}

var gen = g();
var g1 = gen.next();
var g2 = gen.next();
gc();
assertEq(g1(1), 2);
assertEq(g2(1), 3);
gc();
assertEq(g1(1), 4);
assertEq(g2(1), 5);
gen = g1 = g2 = null;
gc();
