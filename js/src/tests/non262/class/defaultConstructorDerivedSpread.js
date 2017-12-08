/* Make sure that the default derived class constructor has the required spread semantics.
 *
 * Test credit André Bargull
 */

Array.prototype[Symbol.iterator] = function*() { yield 1; yield 2; };

class Base {
    constructor(a, b) {
        assertEq(a, 1);
        assertEq(b, 2);
    }
};
class Derived extends Base {};

new Derived();

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
