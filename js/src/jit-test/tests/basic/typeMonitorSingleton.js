// |jit-test| error: TypeError

var x = {f:20};

function foo() {
  for (var i = 0; i < 10; i++)
    x.f;
}
foo();

gc();

// a type barrier needs to be generated for the access on x within foo,
// even though its type set is initially empty after the GC.
x = undefined;

foo();
