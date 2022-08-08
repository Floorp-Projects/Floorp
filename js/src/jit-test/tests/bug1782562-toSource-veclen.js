var z = "1";
try {
    f = function (x) {
        (function(){});
        (function(){});
        (function(){});
        (function(){});
        (function(){});
        (function(){});
        (function(){});
        (function(){});
        (function(){});
        (function(){});
        (function(){});
        (function(){});
        (function(){});
        (function(){});
        (function(){});
        (function(){});
    };
    for (let i = 0; i < 99; i++)
        z += z
} catch (e) {}
uneval(this);
