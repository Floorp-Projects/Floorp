if (typeof(gcPreserveCode) == "function")
	gcPreserveCode();

function testStrict() {
    var n = 10, a = [];
    for (var i = 0; i < 10; ++i) {
        a[0] = (gc());
        a[1] = (n !== 10);
        a[0x2 ] = (n === null);
        a[3] = (n == null);
    }
    return  ;  
}
testStrict();
