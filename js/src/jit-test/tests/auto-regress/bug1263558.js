if (!('oomTest' in this))
    quit();

evalcx(`
    eval('\
        var appendToActual = function(s) {};\
        gczeal = function() {};\
        gcslice = function() {};\
        selectforgc = function() {};\
        if (!("verifyprebarriers" in this)) {\
            verifyprebarriers = function() {};\
        }\
    ');
    oomTest(() => eval('Array(..."")'));
    if ('Intl' in this)
        Intl.NumberFormat.prototype.format(0);
`, newGlobal());
