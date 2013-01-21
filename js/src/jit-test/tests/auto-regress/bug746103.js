// |jit-test| slow; error:InternalError

// Binary: cache/js-dbg-64-c61e7c3a232a-linux
// Flags: -m -n -a
//

gczeal(2);
function testCallProtoMethod() {
    function X() {
        this.valueOf = new testCallProtoMethod( "return this.value" );
    }
    X.prototype.getName = function () { return "X"; }
    var a = [new X, new X, new getName, new new this, new Y];
}
testCallProtoMethod();
