var i = 0;
oomTest(function() {
    for (var j = 0; j < 10; ++j) {
        var r = RegExp(`(?<_${(i++).toString(32)}>a)`);
        r.exec("a");
    }
});
