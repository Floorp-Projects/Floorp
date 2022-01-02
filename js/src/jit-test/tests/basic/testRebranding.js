q = "";
function g() { q += "g"; }
function h() { q += "h"; }
a = [g, g, g, g, h];
for (i=0; i<5; i++) { f = a[i];  f(); }

function testRebranding() {
    return q;
}
assertEq(testRebranding(), "ggggh");
