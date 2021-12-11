var a = '';
var b = '';

function foo() {
    a += 'x';
    b = b + a;
}

with ({}) {}
for (var i = 0; i < 50000; i++) {
    try { foo() } catch {}
}
