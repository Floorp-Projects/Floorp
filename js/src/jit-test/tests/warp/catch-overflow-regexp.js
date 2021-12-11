function test() {
    try {
        test();
    } catch {
        /a/.test("a");
    }
}
test();
