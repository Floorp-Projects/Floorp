// |jit-test| error: ReferenceError
function bitsinbyte(b) {
    while(m<0x100) {    }
}
function TimeFunc(func) {
    for(var y=0; y<256; y++) func(y);
}
function nestedExit2() {
    TimeFunc(bitsinbyte);
}
assertEq(nestedExit2(), "ok");
