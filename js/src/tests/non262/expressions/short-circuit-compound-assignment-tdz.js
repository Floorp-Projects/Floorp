// Test TDZ for short-circuit compound assignments.

// TDZ for lexical |let| bindings.
{
  assertThrowsInstanceOf(() => { let a = (a &&= 0); }, ReferenceError);
  assertThrowsInstanceOf(() => { let a = (a ||= 0); }, ReferenceError);
  assertThrowsInstanceOf(() => { let a = (a ??= 0); }, ReferenceError);
}

// TDZ for lexical |const| bindings.
{
  assertThrowsInstanceOf(() => { const a = (a &&= 0); }, ReferenceError);
  assertThrowsInstanceOf(() => { const a = (a ||= 0); }, ReferenceError);
  assertThrowsInstanceOf(() => { const a = (a ??= 0); }, ReferenceError);
}

// TDZ for parameter expressions.
{
  assertThrowsInstanceOf((a = (b &&= 0), b) => {}, ReferenceError);
  assertThrowsInstanceOf((a = (b ||= 0), b) => {}, ReferenceError);
  assertThrowsInstanceOf((a = (b ??= 0), b) => {}, ReferenceError);
}

// TDZ for |class| bindings.
{
  assertThrowsInstanceOf(() => { class a extends (a &&= 0) {} }, ReferenceError);
  assertThrowsInstanceOf(() => { class a extends (a ||= 0) {} }, ReferenceError);
  assertThrowsInstanceOf(() => { class a extends (a ??= 0) {} }, ReferenceError);
}

// TDZ for lexical |let| bindings with conditional assignment.
{
  assertThrowsInstanceOf(() => {
    const False = false;
    False &&= b;
    b = 2;
    let b;
  }, ReferenceError);

  assertThrowsInstanceOf(() => {
    const True = true;
    True ||= b;
    b = 2;
    let b;
  }, ReferenceError);

  assertThrowsInstanceOf(() => {
    const NonNull = {};
    NonNull ??= b;
    b = 2;
    let b;
  }, ReferenceError);
}

if (typeof reportCompare === "function")
  reportCompare(0, 0);
