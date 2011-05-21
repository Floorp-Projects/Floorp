(function () {
    var x = 1 || 1.2;
    true ? ~x : x;
    x >> x;
})();
