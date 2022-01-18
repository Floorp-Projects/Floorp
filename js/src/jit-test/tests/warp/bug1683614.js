// |jit-test| --ion-offthread-compile=off; --ion-full-warmup-threshold=0; --warp-async; --baseline-eager; skip-if: !this.hasOwnProperty("ReadableStream")
//
// The following testcase crashes on mozilla-central revision 20201219-3262affdccf6 (debug build, run with --fuzzing-safe --ion-offthread-compile=off --ion-full-warmup-threshold=0 --warp-async --baseline-eager):
gczeal(9, 8);
function s() { }
new ReadableStream({
    start() {
        test();
    }
});
async function test() {
    for (let i17 = 1; i17 <= 30; i17++)
        await s(0 + function () { return i17 });
}