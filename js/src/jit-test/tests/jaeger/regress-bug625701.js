gczeal(2);

for(var i=0; i<20; i++) {
    function f() {
        for (var j = 0; j < 3; j++) {
            (function() {})();
        }
    }
    f();
}
