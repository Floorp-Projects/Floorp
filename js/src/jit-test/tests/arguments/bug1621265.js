// |jit-test| error:null

function f(a) {
  // Add |arguments[0]| to mark the function as having an arguments
  // access. Even though there's a |JSOp::SetArg| bytecode is present, we can
  // still use lazy arguments here, because the |JSOp::SetArg| bytecode is
  // always unreachable.
  var v = arguments[0];
  assertEq(v, 1);

  // Anything below the |throw| is unreachable.
  throw null;

  // Add an unreachable |JSOp::SetArg| bytecode.
  a = 4;
}
f(1);
