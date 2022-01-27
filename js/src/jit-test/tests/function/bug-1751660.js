function foo() {}

function bar(o) {
  function nested() {
    with (o) {
      return Object(...arguments);
    }
  }

  // We need an arbitrary IC before the OSR loop.
  foo();

  // Trigger on-stack-replacement.
  for(let i = 0; i < 100; i++) {}

  // Make the call.
  return nested();
}

// Trigger OSR compilation.
for (var i = 0; i < 5; i++) {
  bar({});
}

// Call passing in the function itself.
print(bar(bar));
