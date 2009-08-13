function bitsinbyte(b) {
    var m = 1, c = 0;
    while(m<0x100) {
        if(b & m) c++;
        m <<= 1;
    }
    return 1;
}
function TimeFunc(func) {
    var x,y;
    for(var y=0; y<256; y++) func(y);
}
function nestedExit2() {
    TimeFunc(bitsinbyte);
    return "ok";
}
assertEq(nestedExit2(), "ok");
