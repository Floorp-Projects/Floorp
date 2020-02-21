// Static field initialisers can access the inner name binding for class definitions.
{
  class C {
    static field = C;
  }
  assertEq(C.field, C);
}
{
  let C = class Inner {
    static field = Inner;
  };
  assertEq(C.field, C);
}

// Static field initialisers can't access the outer name binding for class expressions
// before it has been initialised.
{
  assertThrowsInstanceOf(() => {
    let C = class {
      static field = C;
    };
  }, ReferenceError);
}

// Static field initialisers can access the outer name binding for class expressions after
// the binding has been initialised
{
  let C = class {
    static field = () => C;
  };
  assertEq(C.field(), C);
}

// Static field initialiser expressions always resolve the inner name binding.
{
  class C {
    static field = () => C;
  }
  assertEq(C.field(), C);

  const D = C;
  C = null;

  assertEq(D.field(), D);
}
{
  let C = class Inner {
    static field = () => Inner;
  }
  assertEq(C.field(), C);

  const D = C;
  C = null;

  assertEq(D.field(), D);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
