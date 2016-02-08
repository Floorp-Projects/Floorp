gczeal(12);

var length = 10000;
var array = new Array(length);
array.fill(null);

// Promote the array to the tenured heap, if it isn't already there.
minorgc();

for (var i = 0; i < length; i++) {
  // Exercise that barrier with some fresh nursery object references!
  array[i] = {};
}

minorgc();

for (var i = length; i > 0; i--) {
  array[i - 1] = {};
}

minorgc();

for (var i = 0; i < length; i++) {
  array[Math.floor(Math.random() * length)] = {};
}

gc();
