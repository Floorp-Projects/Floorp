if (!('oomTest' in this))
    quit();
x = evalcx("lazy");
oomTest(function () {
    x.eval("1");
});
