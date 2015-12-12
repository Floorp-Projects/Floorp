class func extends Function { }
let inst = new func("x", "return this.bar + x");

// First, ensure that we get sane prototype chains for the bound instance
let bound = inst.bind({bar: 3}, 4);
assertEq(bound instanceof func, true);
assertEq(bound(), 7);

// Check the corner case for Function.prototype.bind where the function has
// a null [[Prototype]]
Object.setPrototypeOf(inst, null);
bound = Function.prototype.bind.call(inst, {bar:1}, 3);
assertEq(Object.getPrototypeOf(bound), null);
assertEq(bound(), 4);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
