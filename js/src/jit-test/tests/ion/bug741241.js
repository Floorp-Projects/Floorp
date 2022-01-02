
function f1() {};
new f1();

function f2() {
    try{} catch (x) {}
};
new f2();

function f3() {
    for (var j = 0; j < 100; j++) {}
};
new f3();

function notEager1(){
    function g1() {};
    new g1();
}
for (var i = 0; i < 100; i++)
    notEager1();

function notEager2(){
    function g2() {};
    new g2();
}
for (var i = 0; i < 100; i++)
    notEager2();

function notEager3(){
    function g3() {
        for (var j = 0; j < 100; j++) {}
    };
    new g3();
}
for (var i = 0; i < 100; i++)
    notEager3();
