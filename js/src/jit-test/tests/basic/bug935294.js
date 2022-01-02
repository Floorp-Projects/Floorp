// |jit-test| error: ReferenceError
for (var c in foo)
  try {
    throw new Error();
  } catch (e)  {}
