obj = new Proxy(Number.bind(), {});
Object.defineProperty(obj, "caller", {set: function () {}});
