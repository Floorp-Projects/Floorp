var SECTION = "15.4.5.2-2";
addCase(new Array, 0, Math, Math.pow(2, SECTION));
var arg = "", i = 0;
var a = eval("new Array(" + arg + ")");
addCase(a, i, +i + 1, Math.pow(2, 12) + i + 1, true);
function addCase(object, old_len, set_len, new_len, checkitems) {
    for (var i = old_len; i < new_len; i++) if (object[i] != 0) {}
}
