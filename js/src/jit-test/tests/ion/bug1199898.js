// |jit-test| error: TypeError
do {
  for (var a of [{}]) {}
} while (4());
