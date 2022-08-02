// |jit-test| --ion-offthread-compile=off; --ion-full-warmup-threshold=0; --baseline-eager; skip-if: !this.hasOwnProperty("ReadableStream")

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