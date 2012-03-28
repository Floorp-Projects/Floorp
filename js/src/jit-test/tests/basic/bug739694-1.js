function newFunc(x) { new Function(x)(); }; try { newFunc("\
var g = newGlobal('new-compartment');\
g.h = function () {\
    names = foo.blaaaaaaaaaaaaah().environment.names();\
};\
g.eval('var obj = {a: 1};' + \"with ({a: 1, '0xcafe': 2, ' ': 3, '': 4, '0': 5}) h();\");\
");
} catch(exc1) {}
function newFunc(x) { new Function(x)(); }; newFunc("\
gczeal(2);\
  a=b=c=d=0; this.__defineGetter__('g', gc); for each (y in this);\
");
