// |jit-test| error: uncaught exception:
eval("\
    try {\
        throw\"\"\
    } catch (\
        x if (function(){\
            x\
        })()\
    ) {}\
")
