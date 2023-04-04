// |jit-test| --ion-pruning=off; --fast-warmup

function foo() {
    Date.prototype.toLocaleString()
}

for (var i = 0; i < 100; i++) {
    try {
	foo();
    } catch {}
}
