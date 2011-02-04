var t;
(function () {
    t = (function() {
            yield k();
        })();
    function h() {
    }
    function k() {
        return function() { h(); };
    }
})();
t.next();

