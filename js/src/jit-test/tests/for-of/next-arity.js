// For-of passes one arg to "next".

load(libdir + 'iteration.js')

var log = '';

function Iter() {
    function next() {
        log += 'n';
        assertEq(arguments.length, 0)
        assertEq(arguments[0], undefined)
        return { get value() { throw 42; }, done: true }
    }

    this[Symbol.iterator] = function () { return this; }
    this.next = next;
}

for (var x of new Iter())
    throw 'not reached';

assertEq(log, 'n');
