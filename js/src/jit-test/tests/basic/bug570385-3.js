var actual = '';

var a = [0, 1];
a.x = 10;
delete a.x;
for (var i in a) {
    appendToActual(i);
    a.pop();
}

assertEq(actual, "0,");
