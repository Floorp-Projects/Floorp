with ({}) {}

function test(s) {
    "".replace(/x/, s);
}

for (var i = 0; i < 20; i++) {
    test("");
}

try {
    test(new Proxy({}, { get: invalidate }));
} catch {}
