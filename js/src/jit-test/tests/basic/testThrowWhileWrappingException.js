var caught = false;
try {
    evalcx("eval(\"throw<x/>\")", Function.a)
} catch (e) {
    caught = true;
}
assertEq(caught, true);
