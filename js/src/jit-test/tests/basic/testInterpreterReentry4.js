function testInterpreterReentry4() {
    var obj = {a:1, b:1, c:1, d:1, get e() { return 1000; } };
    for (var p in obj)
        obj[p];
}
testInterpreterReentry4();
