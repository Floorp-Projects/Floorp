// Bug 1098132: Shouldn't assert.

options('strict');
function eval() {};
eval();
