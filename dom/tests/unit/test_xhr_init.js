function run_test()
{
    Components.utils.importGlobalProperties(["XMLHttpRequest"]);

    var x = new XMLHttpRequest({mozAnon: true, mozSystem: false});
    Assert.ok(x.mozAnon);
    Assert.ok(x.mozSystem); // Because we're system principal

    x = new XMLHttpRequest({mozAnon: true});
    Assert.ok(x.mozAnon);
    Assert.ok(x.mozSystem);

    x = new XMLHttpRequest();
    Assert.ok(!x.mozAnon);
    Assert.ok(x.mozSystem);
}
