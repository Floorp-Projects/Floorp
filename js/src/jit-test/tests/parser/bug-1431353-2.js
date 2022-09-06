// |jit-test| skip-if: helperThreadCount() === 0

// Test off-thread parsing correctly fixes up prototypes of special objects when
// merging back to the target compartment.

function execOffThread(source)
{
    offThreadCompileToStencil(source);
    var stencil = finishOffThreadStencil();
    return evalStencil(stencil);
}

function parseModuleOffThread(source)
{
    offThreadCompileModuleToStencil(source);
    var stencil = finishOffThreadStencil();
    return instantiateModuleStencil(stencil);
}

let a = { x: 1 };
let b = execOffThread("undefined, { x: 1 }")
let c = execOffThread("undefined, { x: 1 }")

assertEq(Object.getPrototypeOf(a), Object.prototype);
assertEq(Object.getPrototypeOf(b), Object.prototype);
assertEq(Object.getPrototypeOf(c), Object.prototype);

a = () => 1;
b = execOffThread("() => 1")
c = execOffThread("() => 1")

assertEq(Object.getPrototypeOf(a), Function.prototype);
assertEq(Object.getPrototypeOf(b), Function.prototype);
assertEq(Object.getPrototypeOf(c), Function.prototype);

a = [1, 2, 3];
b = execOffThread("[1, 2, 3]")
c = execOffThread("[1, 2, 3]")

assertEq(Object.getPrototypeOf(a), Array.prototype);
assertEq(Object.getPrototypeOf(b), Array.prototype);
assertEq(Object.getPrototypeOf(c), Array.prototype);

a = /a/;
b = execOffThread("/a/")
c = execOffThread("/a/")

assertEq(Object.getPrototypeOf(a), RegExp.prototype);
assertEq(Object.getPrototypeOf(b), RegExp.prototype);
assertEq(Object.getPrototypeOf(c), RegExp.prototype);

a = parseModule("");
b = parseModuleOffThread("");
c = parseModuleOffThread("");

assertEq(Object.getPrototypeOf(b), Object.getPrototypeOf(a));
assertEq(Object.getPrototypeOf(c), Object.getPrototypeOf(a));
