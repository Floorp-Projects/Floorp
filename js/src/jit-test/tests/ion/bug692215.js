function test1() {
    var src = "switch(x) {\n";
    for (var i=-1; i<4; i++) {
        src += (i >= 0) ? src : "default:\n";
    }
}
test1();
