// Resolve ArrayBuffer before OOM-testing, so OOM-testing runs less code and is
// less fragile.
var AB = ArrayBuffer;

function f()
{
  return new AB(256);
}

// Create |f|'s script before OOM-testing for the same reason.
f();

oomTest(f);
