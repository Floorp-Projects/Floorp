// |jit-test| error: InternalError
function rec(x, self) {
    if (a = parseLegacyJSON("[1 , ]").length)
        self(x - 001 , self);
    self(NaN, self);
}
rec(1, rec);
