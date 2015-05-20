var test = `

class base {
    constructor() {}

    getValue() {
        return this._prop;
    }

    setValue(v) {
        this._prop = v;
    }
}

class derived extends base {
    constructor() {}

    get a() { return super.getValue(); }
    set a(v) { super.setValue(v); }

    get b() { return eval('super.getValue()'); }
    set b(v) { eval('super.setValue(v);'); }

    test() {
        this.a = 15;
        assertEq(this.a, 15);

        assertEq(this.b, 15);
        this.b = 30;
        assertEq(this.b, 30);
    }
}

var derivedInstance = new derived();
derivedInstance.test();

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
