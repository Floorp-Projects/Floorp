var Test = function (foo) {
    var a = [];

    this.fillArray = function() {
        a = [];
        for (var i = 0; i < 10; i++)
            a.push(0);
        assertEq(a.length, 10);
    }

    foo.go(this);
};

// Import assertEq now to prevent global object shape from changing.
assertEq(true, true);

(new Test({ go: function(p) {
    p.fill = function() {
        p.fillArray();
    }
}})).fill();

new Test({ go: function(p) {
    for (var k = 0; k < 10; k++)
        p.fillArray();
}});
