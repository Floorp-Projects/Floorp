load(libdir + "asserts.js");
obj = wrap(Number.bind());
assertThrowsInstanceOf(function () { Object.defineProperty(obj, "caller", {set: function(){}}); }, TypeError);

