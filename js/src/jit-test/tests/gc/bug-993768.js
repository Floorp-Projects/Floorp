var SECTION = "";
gcPreserveCode()
gczeal(9, 1000);
function test() {
    var f32 = new Float32Array(10);
    f32[0] = 5;
    var i = 0;
    for (var j = 0; j < 10000; ++j) {
        f32[i + 1] = f32[i] - 1;
        SECTION += 1;
    }
} 
test();
