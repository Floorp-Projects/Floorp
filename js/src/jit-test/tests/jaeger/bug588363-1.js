// |jit-test| error: SyntaxError
({eval} = Object.defineProperty(evalcx("lazy"), "", {}))
eval("eval(/x/)", [])

/* Don't assert or crash. */

