function spin_loop()
{
    for (let i = 0; i < 10000; i++) ;
}

function check_timing(label, count) {
    if (count == -1) {
	print("TEST-UNEXPECTED-FAIL | TestPerf | " + label);
	throwError();
    } else {
	print("TEST-PASS | TestPerf | " + label + " = " + count);
    }
}

var pm = new PerfMeasurement(PerfMeasurement.ALL);
if (pm.eventsMeasured == 0) {
    print("TEST-KNOWN-FAIL | perf-smoketest | stub, skipping test");
} else {
    pm.start();
    spin_loop();
    pm.stop();

    check_timing("cpu_cycles", pm.cpu_cycles);
    check_timing("instructions", pm.instructions);
    check_timing("cache_references", pm.cache_references);
    check_timing("cache_misses", pm.cache_misses);
    check_timing("branch_instructions", pm.branch_instructions);
    check_timing("branch_misses", pm.branch_misses);
    check_timing("bus_cycles", pm.bus_cycles);
    check_timing("page_faults", pm.page_faults);
    check_timing("major_page_faults", pm.major_page_faults);
    check_timing("context_switches", pm.context_switches);
    check_timing("cpu_migrations", pm.cpu_migrations);
}
