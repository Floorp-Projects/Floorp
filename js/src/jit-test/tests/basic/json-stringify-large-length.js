// |jit-test| error:InternalError
var arr = [1, 2, 3];
var proxy = new Proxy(arr, {
    get(target, prop) {
        if (prop === "length") {
            return Math.pow(2, 33);
        }
    }
});
JSON.stringify(proxy);
