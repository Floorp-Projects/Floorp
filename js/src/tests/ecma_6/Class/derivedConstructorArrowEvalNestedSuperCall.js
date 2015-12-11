new class extends class { } {
    constructor() {
        (()=>eval("super()"))();
        assertEq(this, eval("this"));
        assertEq(this, (()=>this)());
    }
}();

new class extends class { } {
    constructor() {
        (()=>(()=>super())())();
        assertEq(this, eval("this"));
        assertEq(this, (()=>this)());
    }
}();

new class extends class { } {
    constructor() {
        eval("(()=>super())()");
        assertEq(this, eval("this"));
        assertEq(this, (()=>this)());
    }
}();

new class extends class { } {
    constructor() {
        eval("eval('super()')");
        assertEq(this, eval("this"));
        assertEq(this, (()=>this)());
    }
}();

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
