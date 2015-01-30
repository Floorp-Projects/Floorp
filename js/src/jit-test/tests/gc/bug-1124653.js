var o = {};
gczeal(14);
for (var i = 0; i < 200; i++) {
    with (o) { }
}
