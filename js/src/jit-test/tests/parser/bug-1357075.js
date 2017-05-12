// |jit-test| error: TypeError

var iterable = {};
var iterator = {
  return: 1
};
iterable[Symbol.iterator] = function() {
  return iterator;
};
for ([ class get {} ().iterator ] of [iterable]) {}
