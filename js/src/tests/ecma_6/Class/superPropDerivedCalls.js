var test = `

let derivedInstance;

class base {
    constructor() { }
    method(a, b, c) {
        assertEq(this, derivedInstance);
        this.methodCalled = true;
        assertEq(a, 1);
        assertEq(b, 2);
        assertEq(c, 3);
    }

    get prop() {
        assertEq(this, derivedInstance);
        this.getterCalled = true;
        return this._prop;
    }

    set prop(val) {
        assertEq(this, derivedInstance);
        this.setterCalled = true;
        this._prop = val;
    }
}

class derived extends base {
    constructor() { super(); }

    // |super| actually checks the chain, not |this|
    method() { throw "FAIL"; }
    get prop() { throw "FAIL"; }
    set prop(v) { throw "FAIL"; }

    test() {
        this.reset();
        // While we're here. Let's check on super spread calls...
        let spread = [1,2,3];
        super.method(...spread);
        super.prop++;
        this.asserts();
    }

    testInEval() {
        this.reset();
        eval("super.method(1,2,3); super.prop++");
        this.asserts();
    }

    testInArrow() {
        this.reset();
        (() => (super.method(1,2,3), super.prop++))();
        this.asserts();
    }

    reset() {
        this._prop = 0;
        this.methodCalled = false;
        this.setterCalled = false;
        this.getterCalled = false;
    }

    asserts() {
        assertEq(this.methodCalled, true);
        assertEq(this.getterCalled, true);
        assertEq(this.setterCalled, true);
        assertEq(this._prop, 1);
    }

}

derivedInstance = new derived();
derivedInstance.test();
derivedInstance.testInEval();
derivedInstance.testInArrow();

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
