// Results from another compartment are correctly interpreted by for-of.

load(libdir + "iteration.js");

var g = newGlobal();
g.eval("var obj = {};\n" +
       "obj[Symbol.iterator] = function () { return this; };\n" +
       "obj.next = function () { return { done: true }; };\n");
for (x of g.obj)
    throw 'FAIL';
