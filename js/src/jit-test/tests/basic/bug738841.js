try {
    for (let z = 0; z < 1; ++evalcx("[]", newGlobal())) {}
} catch (e) {}
try {
    for (y in [schedulegc(58)]) {
        b
    }
} catch (e) {}
try {
    e
} catch (e) {}
try {
    (function() {
        h
    }())
} catch (e) {}
try {
    (function() {
        this.m.f = function() {}
    }())
} catch (e) {}
try {
    t()
} catch (e) {}
try {
    p
} catch (e) {}
try {
    gc()
    p
} catch (e) {}
try {
    (function() {
        for (var v of m) {}
    }())
} catch (e) {}
try {
    m
} catch (e) {}
try {
    var f = function() {
        {
            print(new function(q)("", s))
            let u
        }
    };
    dis(f);
    f();
} catch (e) {}
