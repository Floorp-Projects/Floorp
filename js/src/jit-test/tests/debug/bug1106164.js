// |jit-test| error: ReferenceError
var g = newGlobal();
g.parent = this;
g.eval("new Debugger(parent).onExceptionUnwind = function () { };");
evaluate("\
        var tokenCodes  = {};\
        tokenCodes.continue = 0;\
        var arr = [\
            (0.E87  ), \
        ];\
        for(var reportCompare   in tokenCodes) {\
            for(var p1 in arr) {\
                if(arr[j  . arr      ++    ] === p) {\
            }\
        }\
    }\
", { noScriptRval : true, isRunOnce: true });
