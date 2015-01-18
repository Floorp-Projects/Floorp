function f(x) {
    return x * 3;
}
for (var i=0; i<100; i++) {
    f(i);
}
relazifyFunctions();

f(1);
f(1);

relazifyFunctions();

for (var i=0; i<10; i++) {
    f.call(null, i);
}
