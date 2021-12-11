x = "a";
x += +"a";
x += +"a";
x += x;
x += x;
var s = x;
x += 0;
y = x += 0;
y += x += "a";
for (let i = 0; i < 12; ++i) {
    try {
        this();
    } catch {}
}
