// Test TDZ for optional chaining.

// TDZ for lexical |let| bindings with optional chaining.
{
  assertThrowsInstanceOf(() => {
    const Null = null;
    Null?.[b];
    b = 0;
    let b;
  }, ReferenceError);

  assertThrowsInstanceOf(() => {
    const Null = null;
    Null?.[b]();
    b = 0;
    let b;
  }, ReferenceError);

  assertThrowsInstanceOf(() => {
    const Null = null;
    delete Null?.[b];
    b = 0;
    let b;
  }, ReferenceError);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
