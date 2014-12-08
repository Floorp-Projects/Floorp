function f(x) {
        Math.exp(x ? 0 : 1)
}
f(objectEmulatingUndefined())
f(objectEmulatingUndefined())

