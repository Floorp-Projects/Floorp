function foo() {
    try {
        foo();
        /a/.exec();
    } catch (e) {
    }
}
foo();
