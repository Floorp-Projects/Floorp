// |jit-test| error: TypeError
test = "function f(a) { return a } f`a$b`";
evalWithCache(test, {});
dbg = new Debugger();
gczeal(9, 1);
dbg.findScripts('January 0 0 is invalid');
function evalWithCache(code, ctx) {
    ctx.global = newGlobal();
    evaluate(code, ctx)
}
