if (!('oomTest' in this))
    quit();
oomTest(() => eval(`Array(..."ABC")`));
