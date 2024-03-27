let capturedPrivateAccess;
class A {
  // Declare private name in outer class.
  static #x = 42;

  static [(
    // Inner class in computed property key.
    class {},

    // Access to private name from outer class.
    capturedPrivateAccess = () => A.#x
  )];
}
assertEq(capturedPrivateAccess(), 42);

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
