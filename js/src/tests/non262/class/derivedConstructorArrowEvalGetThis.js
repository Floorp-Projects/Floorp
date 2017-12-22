new class extends class { } {
    constructor() {
        super();
        assertEq(this, (()=>this)());
        assertEq(this, eval("this"));
    }
}();

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
