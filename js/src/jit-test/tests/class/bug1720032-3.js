function main() {
  class Base {}

  class Derived extends Base {
    constructor() {
      super();

      let v = 0xffff;

      try {
        // Ensure this statement doesn't get DCE'ed.
        v &= 0xff;

        // Calling |super()| twice throws an error.
        super();
      } catch {}

      assertEq(v, 255);
    }
  }

  for (let i = 0; i < 15; i++) {
    new Derived();
  }
}
main();
main();
