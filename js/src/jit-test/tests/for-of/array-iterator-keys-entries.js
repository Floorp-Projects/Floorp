// array.keys() returns an iterator over the index of elements
// and array.entries() returns an iterator that yields pairs [index, element].

load(libdir + "iteration.js");

var data = [1, 2, 3, "abc"];

var ki = data.keys();
for (var i = 0; i < data.length; i++)
  assertIteratorResult(ki.next(), i, false);
assertIteratorDone(ki, undefined);

var ei = data.entries();
for (var i = 0; i < data.length; i++)
  assertIteratorResult(ei.next(), [i, data[i]], false);
assertIteratorDone(ei, undefined);
