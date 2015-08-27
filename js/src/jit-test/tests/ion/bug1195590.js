function f(x) {
    try {
        eval(x);
    } catch (e) {}
};
f("enableSPSProfilingWithSlowAssertions();");
f("enableTrackAllocations(); throw Error();");
