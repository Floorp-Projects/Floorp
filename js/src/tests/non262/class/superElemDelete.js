// Make sure we get the proper side effects.
// |delete super[expr]| applies ToPropertyKey on |expr| before throwing.

class base {
    constructor() { }
}

class derived extends base {
    constructor() { super(); }
    testDeleteElem() {
        let sideEffect = 0;
        let key = {
            toString() {
                sideEffect++;
                return "";
            }
        };
        assertThrowsInstanceOf(() => delete super[key], ReferenceError);
        assertEq(sideEffect, 1);
    }
    testDeleteElemPropValFirst() {
        // The deletion error is a reference error, even after munging the prototype
        // chain.
        let key = {
            toString() {
                Object.setPrototypeOf(derived.prototype, null);
                return "";
            }
        };
        delete super[key];
    }
}

class derivedTestDeleteElem extends base {
    constructor() {
        let sideEffect = 0;
        let key = {
            toString() {
                sideEffect++;
                return "";
            }
        };

        assertThrowsInstanceOf(() => delete super[key], ReferenceError);
        assertEq(sideEffect, 0);

        super();

        assertThrowsInstanceOf(() => delete super[key], ReferenceError);
        assertEq(sideEffect, 1);

        Object.setPrototypeOf(derivedTestDeleteElem.prototype, null);

        assertThrowsInstanceOf(() => delete super[key], ReferenceError);
        assertEq(sideEffect, 2);

        return {};
    }
}

var d = new derived();
d.testDeleteElem();
assertThrowsInstanceOf(() => d.testDeleteElemPropValFirst(), ReferenceError);

new derivedTestDeleteElem();

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
