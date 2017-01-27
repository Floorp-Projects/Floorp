function f(x) {
    try {
        eval(x);
    } catch (e) {}
};
f("enableGeckoProfilingWithSlowAssertions();");
f("enableTrackAllocations(); throw Error();");
