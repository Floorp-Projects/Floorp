// Test performing GetModuleNamespace on an errored module.

class MyError {}

function assertThrowsMyError(f)
{
    let caught = false;
    try {
        f();
    } catch (e) {
        caught = true;
        assertEq(e.constructor, MyError);
    }
    assertEq(caught, true);
}

registerModule("a", parseModule(`
    throw new MyError();
`));

let c = registerModule("c", parseModule(`
    import "a";
`));
c.declarationInstantiation();
assertThrowsMyError(() => c.evaluation());

let b = registerModule('b', parseModule(`
    import * as ns0 from 'a'
`));
b.declarationInstantiation();
assertThrowsMyError(() => b.evaluation(b));
