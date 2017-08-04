if (!('oomTest' in this))
    quit();

function test(x) {
    var upvar = "";
    function f() { upvar += ""; }
    if (x > 0)
        test(x - 1);
    eval('');
}

oomTest(() => test(10));
