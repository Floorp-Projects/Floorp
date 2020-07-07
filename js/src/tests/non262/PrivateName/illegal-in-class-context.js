// |reftest| skip-if(!xulRuntime.shell)

// Are private fields enabled?
let privateFields = false;
try {
  Function('class C { #x; }');
  privateFields = true;
} catch (exc) {
  assertEq(exc instanceof SyntaxError, true);
}

if (!privateFields) {
  assertThrowsInstanceOf(() => eval(`class A { #x }`), SyntaxError);
  assertThrowsInstanceOf(() => eval(`class A { #x=10 }`), SyntaxError);
} else {
  assertThrowsInstanceOf(() => eval(`class A { #x; #x; }`), SyntaxError);
}

// No computed private fields
assertThrowsInstanceOf(
    () => eval(`var x = "foo"; class A { #[x] = 20; }`), SyntaxError);


if (typeof reportCompare === 'function') reportCompare(0, 0);
