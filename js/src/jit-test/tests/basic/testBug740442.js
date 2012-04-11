function g1() {}
function g2() 
function Int8Array () {}   
function f1(other) {
    eval("gc(); h = g1");
    for(var i=0; i<20; i++) {
        i = i.name;
    }
}
f1(g2);
