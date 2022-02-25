// |reftest| skip-if(!xulRuntime.shell)

source = `class A {
  // Ensure this name parses. Failure would be an InternalError: Buffer too
  // small
  #℘;
}`;

Function(source);

if (typeof reportCompare === 'function') reportCompare(0, 0);