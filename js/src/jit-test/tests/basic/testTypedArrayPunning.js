function testNonCanonicalNan()
{
    const bytes = 128;
    var buf = new ArrayBuffer(bytes);

    /* create an array of non-canonical double nans */
    var ui8arr = new Uint8Array(buf);
    for (var i = 0; i < ui8arr.length; ++i)
        ui8arr[i] = 0xff;

    var dblarr = new Float64Array(buf);
    assertEq(dblarr.length, bytes / 8);

    /* ensure they are canonicalized */
    for (var i = 0; i < dblarr.length; ++i) {
        var asstr = dblarr[i] + "";
        var asnum = dblarr[i] + 0.0;
        assertEq(asstr, "NaN");
        assertEq(asnum, NaN);
    }

    /* create an array of non-canonical float nans */
    for (var i = 0; i < ui8arr.length; i += 4) {
        ui8arr[i+0] = 0xffff;
        ui8arr[i+1] = 0xffff;
        ui8arr[i+2] = 0xffff;
        ui8arr[i+3] = 0xffff;
    }

    var fltarr = new Float32Array(buf);
    assertEq(fltarr.length, bytes / 4);

    /* ensure they are canonicalized */
    for (var i = 0; i < fltarr.length; ++i) {
        var asstr = fltarr[i] + "";
        var asnum = fltarr[i] + 0.0;
        assertEq(asstr, "NaN");
        assertEq(asnum, NaN);
    }
}

testNonCanonicalNan();
