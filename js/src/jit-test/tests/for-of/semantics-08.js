// Results from another compartment are correctly interpreted by for-of.

load(libdir + "iteration.js");

var g = newGlobal();
g.eval(`
    var obj = {};
    obj[${uneval(std_iterator)}] = function () { return this; };
    obj.next = function () { return { done: true }; };
`);
for (x of g.obj)
    throw 'FAIL';
