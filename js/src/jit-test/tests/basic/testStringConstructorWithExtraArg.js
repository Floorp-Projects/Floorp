function testStringConstructorWithExtraArg() {
    for (let i = 0; i < 5; ++i)
        new String(new String(), 2);
    return "ok";
}
assertEq(testStringConstructorWithExtraArg(), "ok");
