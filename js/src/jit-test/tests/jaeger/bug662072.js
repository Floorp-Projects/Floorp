(function () {
    var x;
    x = arguments.length;
    return function () {
        [1][x = arguments.length];
    };
}).call().apply();
