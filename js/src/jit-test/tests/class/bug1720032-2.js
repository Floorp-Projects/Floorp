function main() {
  class Base {}

  class Derived extends Base {
    constructor() {
      let v = 0xffff;

      try {
        // Ensure this statement doesn't get DCE'ed.
        v &= 0xff;

        // Accessing |this| throws when |super()| wasn't yet called.
        this;
      } catch {}

      assertEq(v, 255);

      super();
    }
  }

  for (let i = 0; i < 15; i++) {
    new Derived();
  }
}
main();
main();
