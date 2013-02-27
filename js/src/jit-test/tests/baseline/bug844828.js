gczeal(2);
eval("\
for (var z = 0; z < 50; z++) {\
    try { (function()  {\
            h\
        })()\
    } catch(e) {}\
}\
");
