var o = { };
for (var i = 0; i <= 50; i++)
    o[i] = i;

Object.defineProperty(o, "51", { get: assertEq });

var threw = 0;
function g(o, i) {
    try {
        assertEq(o[i], i);
    } catch (e) {
        threw++;
    }
}

function f() {
    for (var i = 0; i <= 51; i++)
      g(o, i);
}

f();
f();
f();
assertEq(threw, 3);

