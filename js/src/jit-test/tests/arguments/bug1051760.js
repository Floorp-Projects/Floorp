function test() {
    eval("var { [arguments] : y } = {};");
}
test();
