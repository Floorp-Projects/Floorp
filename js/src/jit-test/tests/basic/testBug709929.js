f = eval("\
    (function() {\
        for each(l in[0,0]) {\
            with(gc()>[this for(x in[function(){}])]) {\
                var a;\
                yield\
            }\
        }\
    })\
")
for (i in f()) {}
