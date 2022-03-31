var a = newGlobal({ newCompartment: true });
Debugger(a);
evaluate("function h() { 'use asm'; return {}}", {
    global: a
});
