// |jit-test| error: uncaught exception:
Function("\
    try {\
        throw\"\"\
    } catch (\
        x if (function(){\
            x\
        })()\
    ) {}\
")()

