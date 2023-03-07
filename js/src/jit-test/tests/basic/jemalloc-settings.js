for (var i = -5; i <= 16; i++) {
    setMallocMaxDirtyPageModifier(i);
    for (var j = 0; j < 20; j++) {
        var arr = Array(j).fill(j);
    }
    gc();
}
