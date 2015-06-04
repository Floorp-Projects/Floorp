// Super property accesses should play nice with the pretty printer.

var test = `

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
