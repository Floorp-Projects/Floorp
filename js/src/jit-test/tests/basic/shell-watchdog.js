// |jit-test| exitstatus: 6; skip-if: getBuildConfiguration("wasi")

/* This test will loop infinitely if the shell watchdog
   fails to kick in. */

timeout(0.1);
var start = new Date();

while (true) {
    var end = new Date();
    var duration = (end.getTime() - start.getTime()) / 1000;
    if (duration > 1) {
        print("tick");
        start = new Date();
    }
}
