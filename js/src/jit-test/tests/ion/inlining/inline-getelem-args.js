function cat() {
    var res = "";
    for (var i = 0; i < arguments.length; i++)
        res += arguments[i];
    return res;
}

function cat_m1(ion) {
    var res = "";
    for (var i = (ion ? -1 : 0); i < arguments.length; i++)
        res += arguments[i];
    return res;
}

function cat_p1(ion) {
    var res = "";
    for (var i = 0; i < arguments.length + (ion ? 1 : 0); i++)
        res += arguments[i];
    return res;
}

function sum() {
    var res = 0;
    for (var i = 0; i < arguments.length; i++)
        res += arguments[i];
    return res;
}

function wrapper() {
    var res;
    var i6 = { valueOf: () => 6 },
        i8 = 8.5,
        s2 = { toString: () => "2" },
        s4 = {},
        s6 = "6",
        s7 = undefined,
        s8 = null;
    for (var b = true; b; b = !inIon()) {
        res = sum(1,2,3,4,5,i6,7,i8,9,10);
        assertEq(res, 55.5);

        res = cat(true,s2,3,s4,5,s6,s7,s8);
        assertEq(res, "true23[object Object]56undefinednull");

        ion = inIon();
        if (typeof ion !== "boolean") break;
        res = cat_m1(ion,1,s2,3,s4,5,s6,s7,s8);
        if (ion) assertEq(res, "undefinedtrue123[object Object]56undefinednull");
        else assertEq(res, "false123[object Object]56undefinednull");

        ion = inIon();
        if (typeof ion !== "boolean") break;
        res = cat_p1(ion,1,s2,3,s4,5,s6,s7,s8);
        if (ion) assertEq(res, "true123[object Object]56undefinednullundefined");
        else assertEq(res, "false123[object Object]56undefinednull");
    }
}

wrapper();
