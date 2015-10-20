if (!('oomTest' in this))
    quit();

var lfcode = new Array();
oomTest(() => getBacktrace({}));
