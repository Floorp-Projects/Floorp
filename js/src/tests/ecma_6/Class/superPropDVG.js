// Super property accesses should play nice with the pretty printer.

var test = `

function assertThrownErrorContains(thunk, substr) {
    try {
        thunk();
        throw new Error("Expected error containing " + substr + ", no exception thrown");
    } catch (e) {
        if (e.message.indexOf(substr) !== -1)
            return;
        throw new Error("Expected error containing " + substr + ", got " + e);
    }
}

class testNonExistent {
    constructor() {
        super["prop"]();
    }
}
assertThrownErrorContains(() => new testNonExistent(), 'super["prop"]');

var ol = { testNonExistent() { super.prop(); } };
assertThrownErrorContains(() => ol.testNonExistent(), "super.prop");

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
