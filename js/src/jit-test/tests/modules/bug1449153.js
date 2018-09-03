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

let moduleRepo = {};
setModuleResolveHook(function(module, specifier) {
    return moduleRepo[specifier];
});

moduleRepo["a"] = parseModule(`
    throw new MyError();
`);

let c = moduleRepo["c"] = parseModule(`
    import "a";
`);
instantiateModule(c);
assertThrowsMyError(() => evaluateModule(c));

let b = moduleRepo['b'] = parseModule(`
    import * as ns0 from 'a'
`);
instantiateModule(b);
assertThrowsMyError(() => evaluateModule(b));
