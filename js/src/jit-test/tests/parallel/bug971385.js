// |jit-test| slow;
load(libdir + "asserts.js");

function f() {
    Array.buildPar(6, function() {});
    f();
}

if (getBuildConfiguration().parallelJS)
  assertThrowsInstanceOf(f, InternalError);
