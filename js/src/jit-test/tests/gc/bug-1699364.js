// |jit-test| allow-overrecursed; skip-if: !getJitCompilerOptions()['blinterp.enable']

gczeal(4);
for (let i = 0; i < 1000; i++) {
  "".padStart(10000).startsWith();
}
