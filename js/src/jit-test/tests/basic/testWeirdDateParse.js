var global = this;
function testWeirdDateParseOuter()
{
    var vDateParts = ["11", "17", "2008"];
    var out = [];
    for (var vI = 0; vI < vDateParts.length; vI++)
        out.push(testWeirdDateParseInner(vDateParts[vI]));
    /* Mutate the global shape so we fall off trace; this causes
     * additional oddity */
    global.x = Math.random();
    return out;
}
function testWeirdDateParseInner(pVal)
{
    var vR = 0;
    for (var vI = 0; vI < pVal.length; vI++) {
        var vC = pVal.charAt(vI);
        if ((vC >= '0') && (vC <= '9'))
            vR = (vR * 10) + parseInt(vC);
    }
    return vR;
}
function testWeirdDateParse() {
    var result = [];
    result.push(testWeirdDateParseInner("11"));
    result.push(testWeirdDateParseInner("17"));
    result.push(testWeirdDateParseInner("2008"));
    result.push(testWeirdDateParseInner("11"));
    result.push(testWeirdDateParseInner("17"));
    result.push(testWeirdDateParseInner("2008"));
    result = result.concat(testWeirdDateParseOuter());
    result = result.concat(testWeirdDateParseOuter());
    result.push(testWeirdDateParseInner("11"));
    result.push(testWeirdDateParseInner("17"));
    result.push(testWeirdDateParseInner("2008"));
    return result.join(",");
}
assertEq(testWeirdDateParse(), "11,17,2008,11,17,2008,11,17,2008,11,17,2008,11,17,2008");
