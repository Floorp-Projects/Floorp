// Don't assert
var obj = {};
var proxy = new Proxy(obj, {
    get preventExtensions() {
        Object.preventExtensions(obj);
    }
});
Object.preventExtensions(proxy);
