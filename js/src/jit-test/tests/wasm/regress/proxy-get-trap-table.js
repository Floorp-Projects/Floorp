// The 'get' handler should be invoked directly when reading fields of the
// descriptor.

assertErrorMessage(() => {
    var desc = {
        element: "anyfunc",
        initial: 1
    };
    var proxy = new Proxy({}, {
        get: true
    });
    Object.setPrototypeOf(desc, proxy);
    let table = new WebAssembly.Table(desc);
}, TypeError, /proxy handler's get trap/);
