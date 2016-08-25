for (var i = 0; i < 200; i++) {
    (function* get(undefined, ...get) {
        g.apply(this, arguments);
    })();
}
