// This should not cause `j` to be redeclared.
({
    foo: function k(j) {
        var j
    }
});
