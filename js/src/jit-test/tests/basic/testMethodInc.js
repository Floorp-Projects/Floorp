for (var i = 0; i < 9; i++) {
    var x = {f: function() {}};
    x.f++;
    assertEq(""+x.f, "NaN");
}
