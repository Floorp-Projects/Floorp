var chrsz   = 8;  /* bits per input character. 8 - ASCII; 16 - Unicode      */
function hex_md5(s){ return binl2hex(core_md5(str2binl(s), s.length * chrsz));}
function core_md5(x, len) {
    var a =  1732584193;
    var b = -271733879;
    var c = -1732584194;
    var d =  271733878;
    for(var i = 0; i < x.length; i += 16)
        c = md5_ff(c, d, a, b, x[i+ 6], 17, -1473231341);
}
function md5_cmn(q, a, b, x, s, t) {
    return safe_add(bit_rol(safe_add(safe_add(a, q), safe_add(x, t)), s),b);
}
function md5_ff(a, b, c, d, x, s, t) {
    return md5_cmn((b & c) | ((~b) & d), a, b, x, s, t);
}
function safe_add(x, y) {
    var lsw = (x & 0xFFFF) + (y & 0xFFFF);
    var msw = (x >> 16) + (y >> 16) + (lsw >> 16);
    return (msw << 16) | (lsw & 0xFFFF);
}
function bit_rol(num, cnt) {
    return (num << cnt) | (num >>> (32 - cnt));
}
function str2binl(str) {
    var bin = Array();
    var mask = (1 << chrsz) - 1;
    for(var i = 0; i < str.length * chrsz; i += chrsz)
        bin[i>>5] |= (str.charCodeAt(i / chrsz) & mask) << (i%32);
    return bin;
}
function binl2hex(binarray) {}
var plainText = "Rebellious subjects, enemies to peace,\n\
Throw your mistemper'd weapons to the ground,\n\
To know our further pleasure in this case,\n\
To old Free-town, our common judgment-place.\n\
Once more, on pain of death, all men depart."
for (var i = 0; i <4; i++)
    plainText += plainText;
var md5Output = hex_md5(plainText);
