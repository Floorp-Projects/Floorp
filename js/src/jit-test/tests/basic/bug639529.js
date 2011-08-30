function f() {}
g = wrap(f);
g.__defineGetter__('toString', f.toString);
g.toString;
