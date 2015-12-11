var test = `

new class extends class { } {
    constructor() {
        super();
        assertEq(this, (()=>this)());
        assertEq(this, eval("this"));
    }
}();

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
