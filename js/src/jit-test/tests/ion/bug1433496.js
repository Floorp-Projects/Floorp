// |jit-test| --spectre-mitigations=on; skip-if: getBuildConfiguration("mips32") || getBuildConfiguration("mips64") || getBuildConfiguration("riscv64")
function f() {
    return arguments[arguments.length];
}
for (var i = 0; i < 10; i++)
    assertEq(f(), undefined);
