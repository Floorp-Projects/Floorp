var g = newGlobal();
g.parent = this;
g.eval(`
    var dbg = new Debugger();
    var parentw = dbg.addDebuggee(parent);
    dbg.onIonCompilation = function (graph) {};
`);
gczeal(7,1);
var findNearestDateBefore = function(start, predicate) {
    var current = start;
    var month = 1000 * 60 * 60 * 24 * 30;
    for (var step = month; step > 0; step = Math.floor(step / 3)) {
        !predicate(current);
        current = new Date(current.getTime() + step);
    }
};
var juneDate = new Date(2000, 5, 20, 0, 0, 0, 0);
var decemberDate = new Date(2000, 11, 20, 0, 0, 0, 0);
var juneOffset = juneDate.getTimezoneOffset();
var decemberOffset = decemberDate.getTimezoneOffset();
var isSouthernHemisphere = (juneOffset > decemberOffset);
var winterTime = isSouthernHemisphere ? juneDate : decemberDate;
var dstStart = findNearestDateBefore(winterTime, function (date) {});
