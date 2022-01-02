// bindtoAsyncStack shouldn't choke on CCWs of functions.

var g = newGlobal();
g.evaluate("function h() {}");
bindToAsyncStack(g.h, { stack: saveStack() })();

bindToAsyncStack(new Proxy(() => {}, { apply: () => {} }),
                 { stack: saveStack() })
();
