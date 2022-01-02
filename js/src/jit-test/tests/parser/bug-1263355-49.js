load(libdir + "iteration.js");
function* f4(get = [1], f2, ...each) {}
it = f4();
assertIteratorResult(it.return(2), 2, true);
