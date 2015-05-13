function run_test()
{
    Components.utils.importGlobalProperties(["XMLHttpRequest"]);

    var x = new XMLHttpRequest({mozAnon: true, mozSystem: false});
    do_check_true(x.mozAnon);
    do_check_true(x.mozSystem); // Because we're system principal

    x = new XMLHttpRequest({mozAnon: true});
    do_check_true(x.mozAnon);
    do_check_true(x.mozSystem);

    x = new XMLHttpRequest();
    do_check_false(x.mozAnon);
    do_check_true(x.mozSystem);
}
