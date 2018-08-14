var BUGNUMBER = 1471371;
var summary = 'Handle OOM in RegExp';

printBugNumber(BUGNUMBER);
printStatus(summary);

if (!('oomTest' in this))
    quit();

oomTest(function () {
    for (var i = 0; i < 10; ++i) {
        try {
            RegExp("", "gimuyz");
        } catch { }
    }
});

if (typeof reportCompare === "function")
    reportCompare(true, true);
