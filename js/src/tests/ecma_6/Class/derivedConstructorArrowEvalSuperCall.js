var test = `

new class extends class { } {
    constructor() {
        assertEq(eval("super(); this"), this);
        assertEq(this, eval("this"));
        assertEq(this, (()=>this)());
    }
}();

new class extends class { } {
    constructor() {
        (()=>super())();
        assertEq(this, eval("this"));
        assertEq(this, (()=>this)());
    }
}();

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
