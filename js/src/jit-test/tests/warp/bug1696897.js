function foo() {
    Object.defineProperty(this, "valueOf", ({value: 0, writable: true}));
    Object.freeze(this);
}

for (var i = 0; i < 100; i++) {
    foo.call('');
}
