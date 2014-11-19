load(libdir + "asm.js");

var byteLength = Function.prototype.call.bind(
    Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, "byteLength").get
);
var m = asmCompile("glob", "s", "b", `
    "use asm";
    var I32 = glob.Int32Array;
    var i32 = new I32(b);
    var len = glob.byteLength;
    function ch(b2) {
        if (len(b2) & 0xffffff || len(b2) <= 0xffffff || len(b2) > 80000000) {
            return false;
        }
        i32 = new I32(b2);
        b = b2;
        return true
    }
    function get(i) {
        i = i | 0;
        return i32[i >> 2] | 0
    }
    return {
        get: get,
        changeHeap: ch
    }
`);
var buf1 = new ArrayBuffer(16777216)
var { get, changeHeap } = asmLink(m, this, null, buf1)
assertEq(changeHeap(new ArrayBuffer(33554432)), true)
assertEq(get(), 0)
assertEq(changeHeap(buf1), true);
get();
