function TestCase( e, a) {
      getTestCaseResult(e, a);
}
function getTestCaseResult(expected, actual) {}
var msPerSecond =  1000;
var TIME_0000  = (function () {  })();
var now = new Date();
var TIME_NOW = now.valueOf();
function msFromTime(t) {
    var ms = t % msPerSecond;
    return ((ms < 0) ? msPerSecond + ms : ms );
}
new TestCase(false,  eval("true, false"));
addTestCase( TIME_NOW );
addTestCase( TIME_0000 );
function addTestCase( t ) {
    for ( m = 0; m <= 1000; m+=100 ) {
        new TestCase(msFromTime(t), (new Date(t)).getMilliseconds());
    }
}
