function f(o) { 
    Object.seal(o); 
}
gc();
if(2 != 2) {
    g = new f(g);
}
with({}) {
    f({});
}
