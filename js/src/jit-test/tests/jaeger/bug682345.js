
for (var i = 0; i <= 0x017f; i++) {
    var U = new Unicode(i);
}
function Unicode(c) {
    u = GetUnicodeValues(c);
    this.upper = u[0];
}
function GetUnicodeValues(c) {
    u = new Array();
    if ((c >= 0x0100 && c < 0x0138) || (c > 0x0149 && c < 0x0178)) try {} finally {
        return;
    }
    return u;
}
