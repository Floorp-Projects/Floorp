// |jit-test| --spectre-mitigations=on
function f() {
    return arguments[arguments.length];
}
for (var i = 0; i < 10; i++)
    assertEq(f(), undefined);
