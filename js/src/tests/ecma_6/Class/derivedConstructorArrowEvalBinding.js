// Make sure it doesn't matter when we make the arrow function
var test = `

new class extends class { } {
    constructor() {
        let arrow = () => this;
        assertThrowsInstanceOf(arrow, ReferenceError);
        super();
        assertEq(arrow(), this);
    }
}();

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
