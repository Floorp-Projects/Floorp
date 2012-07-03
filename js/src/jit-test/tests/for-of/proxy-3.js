// An exception thrown from a proxy trap while getting the .iterator method is propagated.

load(libdir + "asserts.js");

var p = Proxy.create({
    getPropertyDescriptor: function (name) {
        if (name == "iterator")
            throw "fit";
        return undefined;
    }
});
assertThrowsValue(function () { for (var v of p) {} }, "fit");
