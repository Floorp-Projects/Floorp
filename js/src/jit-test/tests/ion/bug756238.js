// |jit-test| error: ReferenceError

outer:
  for (var elem in {x:1})
    if (p > "q")
      continue outer;
