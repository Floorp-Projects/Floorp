// |jit-test| slow; allow-oom; skip-if: !wasmIsSupported() || !hasFunction.oomTest

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
