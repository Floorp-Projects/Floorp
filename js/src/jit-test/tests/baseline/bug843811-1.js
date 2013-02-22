// |jit-test| error: uncaught exception:
evalcx("\
    try {\
        throw\"\"\
    } catch (\
        x if (function(){\
            x\
        })()\
    ) {}\
", newGlobal(""))

