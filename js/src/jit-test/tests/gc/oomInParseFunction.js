if (!('oomTest' in this))
    quit();

oomTest(() => eval("function f() {}"));
