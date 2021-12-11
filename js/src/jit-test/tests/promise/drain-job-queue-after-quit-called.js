function d() {
    quit();
}

function bt() {
    getBacktrace({thisprops: true});
}

d.toString = bt.toString = drainJobQueue.toString = function () {
    if (this === bt)
        return '';
    this();
}

bt();
