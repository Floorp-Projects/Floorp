var r = RegExp("");
var s = "";
s += "".replace(r, Function("x"));
for (var x = 0; x < 5; x++) {
    s += "".replace(r, this);
}
