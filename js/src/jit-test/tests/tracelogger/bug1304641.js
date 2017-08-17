var failed = false;
try {
    newGlobal().startTraceLogger();
    print("z");
} catch (e) {
    failed = true;
}
assertEq(failed, true);
