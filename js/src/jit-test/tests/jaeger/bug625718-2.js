var o3 = new String("foobarbaz");
var o10 = Math;
var o11 = function() {};

function f3(o) { return o; };
function f4(o) { o.g4 = function() {}; };

for(var i=0; i<20; i++) {
    o11[3] = undefined;
    f4(o3);
    f3(o3);
    f4(o11);
    f4(o10);
}
