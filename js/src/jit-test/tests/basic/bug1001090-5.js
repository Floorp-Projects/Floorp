// |jit-test| error: ReferenceError
evalcx("\
    for(x = 0; x < 9; x++) {\
        let y = y.s()\
    }\
", newGlobal())
