// Test performing GetModuleNamespace on an errored module.

class MyError {}

async function assertThrowsMyError(f)
{
    let caught = false;
    try {
        await f();
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
moduleLink(c);
assertThrowsMyError(() => moduleEvaluate(c));

let b = registerModule('b', parseModule(`
    import * as ns0 from 'a'
`));
moduleLink(b);
assertThrowsMyError(() => moduleEvaluate(b));

drainJobQueue();
