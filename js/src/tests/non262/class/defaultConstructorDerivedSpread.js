/* Make sure that the default derived class constructor has the required spread semantics.
 *
 * Test credit André Bargull
 */

// <https://github.com/tc39/ecma262/pull/2216> changed default derived class
// constructors to no longer execute the spread iteration protocol.
Array.prototype[Symbol.iterator] = function*() {
    throw new Error("unexpected call");
};

class Base {
    constructor(a, b) {
        assertEq(a, 1);
        assertEq(b, 2);
    }
};
class Derived extends Base {};

new Derived(1, 2);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
