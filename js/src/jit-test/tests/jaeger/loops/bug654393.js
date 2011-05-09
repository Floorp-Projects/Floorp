var VERSION = "ECMA_2";
DoWhile(
    new DoWhileObject(false, false, false, VERSION)
);
function DoWhileObject( out1, out2, out3, in1 ) {
    this.breakIn = in1
}
function DoWhile(object) {
    do {
        if (object.breakIn) {}
    } while(false);
}
