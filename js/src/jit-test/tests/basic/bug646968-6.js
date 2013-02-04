function test(s) {
    eval(s);
    let (a = ({}).q = 0, x = x)
        assertEq(x, 5);
}
test('var x = 5;');
