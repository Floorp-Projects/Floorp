
var g = newGlobal({
    sameZoneAs: this,
    useWindowProxy: true,
});

g.evaluate(`
    this.data = 7;

    // Getter / Setter
    Object.defineProperty(this, "prop", {
        get: function() { return this.data; },
        set: function(val) { this.data = val; },
    });

    // Getter / Setter ICs
    for (var i = 0; i < 20; ++i) {
        this.data = i;
        assertEq(prop, i);
        prop = i;
        assertEq(this.prop, i);
        this.prop = i;
        assertEq(this.data, i);
    }
`);


// CCW of WindowProxy
for (var i = 0; i < 20; ++i) {
    g.slot = i;
    assertEq(g.slot, i);
}
