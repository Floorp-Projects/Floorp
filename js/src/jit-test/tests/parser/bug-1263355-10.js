// |jit-test| error: TypeError

function f(m, k = class C extends Array { }, p = m()) { }
f()
