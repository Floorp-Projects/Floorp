for (p in this) {
    delete p;
}
for (p in this) {}
evaluate("for(var i=0; i<50; i++) p = 1");
assertEq(p, 1);
