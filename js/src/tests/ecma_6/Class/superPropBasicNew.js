var test = `

class Base {
    constructor() {}
}
class Mid extends Base {
    constructor() {}
    f() { return new super.constructor(); }
}
class Derived extends Mid {
    constructor() {}
}
let d = new Derived();
var df = d.f();
assertEq(df.constructor, Base);

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
