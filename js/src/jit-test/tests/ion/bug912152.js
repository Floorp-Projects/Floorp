
function foo() {
    try {
        this.f = 0;
    } finally {}
}
new foo();
