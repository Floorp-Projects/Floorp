var test = `

class base {
    method() { return 1; }
    *gen() { return 2; }
    static sMethod() { return 3; }
    get answer() { return 42; }
}

// Having a default constructor should work, and also not make you lose
// everything for no good reason

assertEq(Object.getPrototypeOf(new base()), base.prototype);
assertEq(new base().method(), 1);
assertEq(new base().gen().next().value, 2);
assertEq(base.sMethod(), 3);
assertEq(new base().answer, 42);

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
