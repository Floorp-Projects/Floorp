class testForIn {
    constructor() {
        let hits = 0;
        for (super.prop in { prop1: 1, prop2: 2 })
            hits++;
        assertEq(this.prop, "prop2");
        assertEq(hits, 2);
    }
}

new testForIn();


({
    testForOf() {
        let hits = 0;
        for (super["prop"] of [1, 2])
            hits++;
        assertEq(this.prop, 2);
        assertEq(hits, 2);
    }
}).testForOf();

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
