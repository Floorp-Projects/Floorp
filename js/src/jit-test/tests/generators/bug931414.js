// |jit-test| error: is undefined

load(libdir + "iteration.js");

function iterable() {
  var iterable = {};
  iterable[std_iterator] = () => ({next: () => void 0});
  return iterable;
}

(function*(){yield*iterable()}()).next();
