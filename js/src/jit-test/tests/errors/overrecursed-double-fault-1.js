// |jit-test| slow; allow-overrecursed; allow-unhandlable-oom; skip-if: getBuildConfiguration("android")
// Disabled on Android due to harness problems (Bug 1532654)

enableShellAllocationMetadataBuilder();
function a() {
    a();
}
new a;
