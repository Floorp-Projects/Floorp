function run_test()
{
    // Hack around Promise.jsm being stuck on my global
    do_check_false(PromiseDebugging === undefined);
    var res;
    var p = new Promise(function(resolve, reject) { res = resolve });
    var state = PromiseDebugging.getState(p);
    do_check_eq(state.state, "pending");

    do_test_pending();

    p.then(function() {
        var state = PromiseDebugging.getState(p);
        do_check_eq(state.state, "fulfilled");
        do_check_eq(state.value, 5);
	do_test_finished();
    });

    res(5);
}
