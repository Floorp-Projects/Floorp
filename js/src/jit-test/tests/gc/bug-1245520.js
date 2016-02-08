if (!('oomTest' in this))
    quit();

var t = {};
oomTest(() => serialize(t));
