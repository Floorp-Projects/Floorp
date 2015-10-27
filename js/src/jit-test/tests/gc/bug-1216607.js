if (!('oomTest' in this))
    quit();

enableSPSProfilingWithSlowAssertions();
try {
(function() {
   while (n--) {
   }
})();
} catch(exc1) {}
function arrayProtoOutOfRange() {
    function f(obj) {}
    function test() {
        for (var i = 0; i < 1000; i++)
            var r = f(i % 2 ? a : b);
    }
    test();
}
oomTest(arrayProtoOutOfRange);
