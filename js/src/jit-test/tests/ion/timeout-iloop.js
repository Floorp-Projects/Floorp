// |jit-test| exitstatus: 6; skip-if: getBuildConfiguration("wasi")

timeout(1, function() { return false; });

function forever() { for(;;); }
forever();
