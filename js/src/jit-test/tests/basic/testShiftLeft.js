// |jit-test| TMFLAGS: full,fragprofile,treevis; valgrind

/* Test the proper operation of the left shift operator. This is especially
 * important on ARM as an explicit mask is required at the native instruction
 * level. */

load(libdir + 'range.js');

function testShiftLeft()
{
    var r = [];
    var i = 0;
    var j = 0;

    var shifts = [0,1,7,8,15,16,23,24,31];

    /* Samples from the simple shift range. */
    for (i = 0; i < shifts.length; i++)
        r[j++] = 1 << shifts[i];

    /* Samples outside the normal shift range. */
    for (i = 0; i < shifts.length; i++)
        r[j++] = 1 << (shifts[i] + 32);

    /* Samples far outside the normal shift range. */
    for (i = 0; i < shifts.length; i++)
        r[j++] = 1 << (shifts[i] + 224);
    for (i = 0; i < shifts.length; i++)
        r[j++] = 1 << (shifts[i] + 256);

    return r.join(",");
}

assertEq(testShiftLeft(), 
	 "1,2,128,256,32768,65536,8388608,16777216,-2147483648,"+
	 "1,2,128,256,32768,65536,8388608,16777216,-2147483648,"+
	 "1,2,128,256,32768,65536,8388608,16777216,-2147483648,"+
	 "1,2,128,256,32768,65536,8388608,16777216,-2147483648");
