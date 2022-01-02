setJitCompilerOption("ion.forceinlineCaches", 1);

function testGetPropSuper() {
  class C extends class { static p = 0 } {
    static m() {
      return super.p;
    }
  }

  for (var i = 0; i < 20; ++i) {
    var v = C.m();
    assertEq(v, 0);
  }
}
for (var i = 0; i < 2; ++i) testGetPropSuper();
