function bitsinbyte() {
    var c = 0;
    while (false) c = c * 2;
}
function TimeFunc(func) {
    for(var y=0; y<11000; y++)
        func();
}
for (var i=0; i<50; i++)
    TimeFunc(bitsinbyte);
