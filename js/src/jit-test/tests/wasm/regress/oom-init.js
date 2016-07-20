// |jit-test| allow-oom

if (typeof oomTest !== 'function' || !wasmIsSupported())
    quit();

Object.getOwnPropertyNames(this);
s = newGlobal();
evalcx("\
    /x/;\
    oomTest(function() {\
        this[\"\"];\
        void 0;\
        Object.freeze(this);\
        l(undefined)();\
        O;\
        t;\
        0;\
        ({e});\
        i;\
        0;\
        ({ z: p ? 0 : 0});\
        s;\
    });\
", s);
