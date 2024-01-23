// |jit-test| --fuzzing-safe; --baseline-eager; --arm-hwcap=vfp
function f() {};
f();
f();
f();
try {
    print(disnative(f));
} catch (e) {}
