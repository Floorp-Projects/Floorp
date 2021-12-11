// |jit-test| --no-cgc; allow-oom

async function f(x) {
    await await x;
};
for (let i = 0; i < 800; ++i) {
    f();
}
gcparam("maxBytes", gcparam("gcBytes"));
