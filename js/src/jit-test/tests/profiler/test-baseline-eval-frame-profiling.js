
setJitCompilerOption("baseline.warmup.trigger", 10);

function main() {
    for (var i = 0; i < 50; i++)
        eval("for (var j = 0; j < 50; j++) readGeckoProfilingStack();");
}

gczeal(2, 10000);
enableGeckoProfilingWithSlowAssertions();
main();
