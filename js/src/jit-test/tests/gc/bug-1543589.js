// |jit-test| skip-if: getBuildConfiguration()["arm64-simulator"] === true
// This test times out in ARM64 simulator builds.

gczeal(0);
gcparam('minNurseryBytes', 16 * 1024 * 1024);

let a = [];
for (var i = 0; i < 20000; i++) {
    a.push(import("nonexistent.js"));
    Symbol();
}

for (let p of a) {
    p.catch(() => {});
}
