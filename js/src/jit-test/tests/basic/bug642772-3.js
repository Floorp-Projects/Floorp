// Catch memory leaks when enumerating over the global object.

for (let z = 1; z <= 16000; ++z) {
  for each (y in this);
}
