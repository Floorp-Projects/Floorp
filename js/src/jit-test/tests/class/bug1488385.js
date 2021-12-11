// Default class constructors should no longer be marked as self-hosted
// functions. They should be munged to appear in every respect as if they
// originated with the class definition.

load(libdir + 'asserts.js');

function f() {
    return f.caller.p;
}

// Since default constructors are strict mode code, this should get:
// TypeError: access to strict mode caller function is censored
assertThrowsInstanceOf(() => new class extends f {}, TypeError);
