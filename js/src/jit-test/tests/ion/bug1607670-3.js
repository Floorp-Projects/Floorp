let c = 0;
function f1() {
    c++;
}
function LoggingProxyHandlerWrapper(name, handler={}) {
    return new Proxy(handler, {
        get(x, id) {
            return function (...args) {
                return Reflect[id].apply(null, args);
            };
        }
    });
}
function LoggingProxy(name, target) {
    return new Proxy(f1, new LoggingProxyHandlerWrapper(name));
}
function test() {
    let proxy = new LoggingProxy("proto", {});
    for (let i = 0; i < 2000; i++) {
        new proxy();
    }
    assertEq(c, 2000);
}
test();
