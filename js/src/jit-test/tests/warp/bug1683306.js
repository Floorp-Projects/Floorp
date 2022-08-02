// |jit-test| --ion-offthread-compile=off; --ion-full-warmup-threshold=0; --ion-gvn=off; --baseline-eager
//
// Bug 1683306 - Assertion failure: !genObj->hasStackStorage() || genObj->isStackStorageEmpty(), at vm/GeneratorObject.cpp:144

function assert(mustBeTrue, message) { }
assert.sameValue = function (expected) {
    assert._toString(expected)
};
assert._toString = function (value) {
    return String(value);
}
async function fn() {
    for await ([] of []) { }
}

fn();
bailAfter(10);
assert.sameValue();
evaluate("fn();");
