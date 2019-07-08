// Catch memory leaks when enumerating over the global object.

for (let z = 1; z <= 1600; ++z) {
  for (y in this);
}
