var test = `

let superArrow;
let thisArrow;

let thisStash;

class base {
    constructor() {
        // We run this constructor twice as part of the double init check
        if (!thisStash)
            thisStash = {prop:45};
        return thisStash;
    }
}

class foo extends base {
    constructor() {
        superArrow = (()=>super());
        thisArrow = ()=>this;
    }
}

// Populate the arrow function saves. Since we never invoke super(), we throw
assertThrowsInstanceOf(()=>new foo(), ReferenceError);

// No |this| binding in the closure, yet
assertThrowsInstanceOf(thisArrow, ReferenceError);

// call super()
superArrow();

// Can't call it twice
assertThrowsInstanceOf(superArrow, ReferenceError);

// Oh look, |this| is populated, now.
assertEq(thisArrow(), thisStash);

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
