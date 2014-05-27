if (getBuildConfiguration().parallelJS) {
    Function("\
        Array.buildPar(6947, (function() {\
            return {\
                a: new Function,\
                b1: function() {},\
                b2: function() {},\
                b3: function() {},\
                b4: function() {}\
            }\
        }));\
        selectforgc(new Object);\
    ")()
}
