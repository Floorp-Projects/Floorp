// |reftest| skip-if(!xulRuntime.shell)

source = `class A {
  // Ensure this name parses. Failure would be an InternalError: Buffer too
  // small
  #℘;
}`;

try {
  Function(source);
} catch (e) {
  assertEq(getRealmConfiguration()['privateFields'], false);
  assertEq(e instanceof SyntaxError, true);
}

if (typeof reportCompare === 'function') reportCompare(0, 0);