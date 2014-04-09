
var handler = {
    has: function (name) {
        assertEq(1, 2);
    }
};

for (var i=0; i<10; i++) {
    var regex = /undefined/;
    regex.__proto__ = Proxy.createFunction(handler, function(){})
}

