// |jit-test| need-for-each


s = newGlobal()
function f(code) {
    evalcx(code, s)
}
f("\
    c = [];\
    var x;\
    for each(z in[\
        x,,[],,new Number,,,,new Number,,,,new Number,new Number,[],\
        ,,,[],,new Number,,new Number,,[],new Number,[],,,,,,[],\
        new Number,,new Number,[],,[],,,,[],,[],,,,,,,,,[],[],,[],\
        [],[],,new Number,[],[],,[],,new Number,new Number,new Number,\
        new Number,new Number,,,new Number,new Number,,[],[],[],,,[],\
        [],[],new Number,,new Number,,,,,[],new Number,new Number,[],\
        [],[],[],,x,[]]\
    ) {\
        c = z\
    };\
");
f("c");
