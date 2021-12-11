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

// Check that we actually pass the proper new.target when calling super()
function toBind() { }

var boundArgs = [];
for (let i = 0; i < 5; i++) {
    boundArgs.push(i);
    let bound = toBind.bind(undefined, ...boundArgs);

    // We have to wire it up by hand to allow us to use a bound function
    // as a superclass, but it's doable.
    bound.prototype = {};
    class test extends bound { };
    let passedArgs = [];
    for (let j = 0; j < 15; j++) {
        passedArgs.push(j);
        assertEq(Object.getPrototypeOf(new test(...passedArgs)), test.prototype);
    }
}


if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
