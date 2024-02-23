// |jit-test| skip-if: !hasFunction.oomTest

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
        new Intl.NumberFormat().format(0);
`, newGlobal());
