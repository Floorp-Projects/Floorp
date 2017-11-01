// |jit-test| error: can't clone
var gv = newGlobal();
gv.f = (class get {});
gv.eval('f = clone(f);');
