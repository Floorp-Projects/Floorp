function testConstructorBail() {
    for (let i = 0; i < 5; ++i) new Number(/x/);
}
testConstructorBail();
