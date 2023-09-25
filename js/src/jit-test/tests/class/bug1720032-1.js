// |jit-test| skip-if: getBuildConfiguration("osx") && getBuildConfiguration("arm64")
load(libdir + "asserts.js");
function main() {
  class Base {}

  class Derived extends Base {
    constructor() {
      super();

      let v = 0xffff;

      try {
        // Ensure this statement doesn't get DCE'ed.
        v &= 0xff;

        // Returning a primitive value throws.
        return 0;
      } catch {}

      assertEq(v, 255);
    }
  }

  for (let i = 0; i < 15; i++) {
    assertThrowsInstanceOf(() => new Derived(), TypeError);
  }
}
main();
main();
