var o = {};

o.__defineGetter__(<x/>,function(){});
<a b="2"/>.(o.__defineGetter__(new QName("foo"), function(){}));

var i = 0;
for (w in o) {
    ++i;
}

assertEq(i, 2);
