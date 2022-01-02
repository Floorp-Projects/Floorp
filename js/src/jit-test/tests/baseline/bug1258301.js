x = new WeakMap;
x.__proto__ = null;
for (var i = 0; i < 3; i++)
    x.someprop;
gc();
