var buf = serialize(-1);
var nbuf = serialize(undefined);
for (var j = 0 ; j < 5; j++)
    buf[j + 8] = nbuf[j];
