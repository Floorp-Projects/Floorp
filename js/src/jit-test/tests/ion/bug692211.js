// |jit-test| error: TypeError
function TimeFunc(func) {
    for(var y=0; y<256; y++) func(y);
}
function nestedExit2() {
    TimeFunc(TimeFunc);
}
assertEq(nestedExit2(), "ok");
