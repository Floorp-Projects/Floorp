
f = eval("(function(){return x=Iterator(/x/)})")
for (a in f()) {}
for (d in x) {}

reportCompare(0, 0, "");
