// |reftest| skip-if(!this.hasOwnProperty("Type"))
var BUGNUMBER = 578700;
var summary = 'BinaryData memory check';

function spin() {
    for (var i = 0; i < 10000; i++)
        ;
}

function runTests() {
    print(BUGNUMBER + ": " + summary);

    var AA = new ArrayType(new ArrayType(uint8, 5), 5);
    var aa = new AA();
    var aa0 = aa[0];
    aa[0] = [0,1,2,3,4];

    aa = null;

    gc();
    spin();

    for (var i = 0; i < aa0.length; i++)
        assertEq(aa0[i], i);

    var AAA = new ArrayType(AA, 5);
    var aaa = new AAA();
    var a0 = aaa[0][0];

    for (var i = 0; i < a0.length; i++)
        assertEq(a0[i], 0);

    aaa[0] = [[0,1,2,3,4], [0,1,2,3,4], [0,1,2,3,4], [0,1,2,3,4], [0,1,2,3,4]];

    aaa = null;
    gc();
    spin();
    for (var i = 0; i < a0.length; i++)
        assertEq(a0[i], i);

    if (typeof reportCompare === "function")
        reportCompare(true, true);
    print("Tests complete");
}

runTests();
