setJitCompilerOption("ion.warmup.trigger", 50);
setJitCompilerOption("offthread-compilation.enable", 0);
gcPreserveCode();

try {
    while (true) {
        a = inIon() ? 0 : 300;
        try {
            buf = new Uint8ClampedArray(a);
            assertEq(buf.length, 300);
        } catch (e) {
            assertEqa;
        }
    }
} catch(exc1) {}
loadFile()
function loadFile() {
    try {
        switch (lfRunTypeId) {
          case 1:
            eval();
        }
    } catch (lfVare) {}
}
