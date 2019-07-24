function f(x) {
    return f.toString();
}

monitorType(f, 0, "unknown");
monitorType(f, 1, "unknownObject");
monitorType(f, 2, Symbol());
monitorType(f, 2, this);
monitorType(null, 0, 1);

for (var i = 0; i < 12; i++) {
    monitorType(f, 3, {});
    monitorType(undefined, 20, this);
    f(i);
}

function h() {
    monitorType(g, 1, {});
    monitorType(g, 2, "unknown");
    monitorType(null, 1, {});
}
function g() {
    return new h();
}
for (var i = 0; i < 12; i++) {
    new g();
}
