var total;

if (typeof total == "undefined")
    total = 0;
print("getProp_bench");

// get the same property from the same object, passed as a parameter
function getProp_1(p)
{
    var t;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1;
        t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1;
        t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1;
        t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1;
        t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1;
        t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1;
        t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1;
        t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1;
        t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1;
        t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1;
        t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1;
        t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1;
        t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1;
        t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1;
        t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1;
        t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1;
        t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1;
        t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1;
        t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1;
        t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1; t = p.prop1;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("getProp_1 : " + elapsedTime);
    total += elapsedTime;
}

// get different properties from the same object, passed as a parameter
function getProp_1a(p)
{
    var t0, t1, t2, t3, t4, t5, t6, t7, t8, t9;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t0 = p.prop0; t1 = p.prop1; t2 = p.prop2; t3 = p.prop3; t4 = p.prop4; t5 = p.prop5; t6 = p.prop6; t7 = p.prop7; t8 = p.prop8; t9 = p.prop9;
        t0 = p.prop0; t1 = p.prop1; t2 = p.prop2; t3 = p.prop3; t4 = p.prop4; t5 = p.prop5; t6 = p.prop6; t7 = p.prop7; t8 = p.prop8; t9 = p.prop9;
        t0 = p.prop0; t1 = p.prop1; t2 = p.prop2; t3 = p.prop3; t4 = p.prop4; t5 = p.prop5; t6 = p.prop6; t7 = p.prop7; t8 = p.prop8; t9 = p.prop9;
        t0 = p.prop0; t1 = p.prop1; t2 = p.prop2; t3 = p.prop3; t4 = p.prop4; t5 = p.prop5; t6 = p.prop6; t7 = p.prop7; t8 = p.prop8; t9 = p.prop9;
        t0 = p.prop0; t1 = p.prop1; t2 = p.prop2; t3 = p.prop3; t4 = p.prop4; t5 = p.prop5; t6 = p.prop6; t7 = p.prop7; t8 = p.prop8; t9 = p.prop9;
        t0 = p.prop0; t1 = p.prop1; t2 = p.prop2; t3 = p.prop3; t4 = p.prop4; t5 = p.prop5; t6 = p.prop6; t7 = p.prop7; t8 = p.prop8; t9 = p.prop9;
        t0 = p.prop0; t1 = p.prop1; t2 = p.prop2; t3 = p.prop3; t4 = p.prop4; t5 = p.prop5; t6 = p.prop6; t7 = p.prop7; t8 = p.prop8; t9 = p.prop9;
        t0 = p.prop0; t1 = p.prop1; t2 = p.prop2; t3 = p.prop3; t4 = p.prop4; t5 = p.prop5; t6 = p.prop6; t7 = p.prop7; t8 = p.prop8; t9 = p.prop9;
        t0 = p.prop0; t1 = p.prop1; t2 = p.prop2; t3 = p.prop3; t4 = p.prop4; t5 = p.prop5; t6 = p.prop6; t7 = p.prop7; t8 = p.prop8; t9 = p.prop9;
        t0 = p.prop0; t1 = p.prop1; t2 = p.prop2; t3 = p.prop3; t4 = p.prop4; t5 = p.prop5; t6 = p.prop6; t7 = p.prop7; t8 = p.prop8; t9 = p.prop9;
        t0 = p.prop0; t1 = p.prop1; t2 = p.prop2; t3 = p.prop3; t4 = p.prop4; t5 = p.prop5; t6 = p.prop6; t7 = p.prop7; t8 = p.prop8; t9 = p.prop9;
        t0 = p.prop0; t1 = p.prop1; t2 = p.prop2; t3 = p.prop3; t4 = p.prop4; t5 = p.prop5; t6 = p.prop6; t7 = p.prop7; t8 = p.prop8; t9 = p.prop9;
        t0 = p.prop0; t1 = p.prop1; t2 = p.prop2; t3 = p.prop3; t4 = p.prop4; t5 = p.prop5; t6 = p.prop6; t7 = p.prop7; t8 = p.prop8; t9 = p.prop9;
        t0 = p.prop0; t1 = p.prop1; t2 = p.prop2; t3 = p.prop3; t4 = p.prop4; t5 = p.prop5; t6 = p.prop6; t7 = p.prop7; t8 = p.prop8; t9 = p.prop9;
        t0 = p.prop0; t1 = p.prop1; t2 = p.prop2; t3 = p.prop3; t4 = p.prop4; t5 = p.prop5; t6 = p.prop6; t7 = p.prop7; t8 = p.prop8; t9 = p.prop9;
        t0 = p.prop0; t1 = p.prop1; t2 = p.prop2; t3 = p.prop3; t4 = p.prop4; t5 = p.prop5; t6 = p.prop6; t7 = p.prop7; t8 = p.prop8; t9 = p.prop9;
        t0 = p.prop0; t1 = p.prop1; t2 = p.prop2; t3 = p.prop3; t4 = p.prop4; t5 = p.prop5; t6 = p.prop6; t7 = p.prop7; t8 = p.prop8; t9 = p.prop9;
        t0 = p.prop0; t1 = p.prop1; t2 = p.prop2; t3 = p.prop3; t4 = p.prop4; t5 = p.prop5; t6 = p.prop6; t7 = p.prop7; t8 = p.prop8; t9 = p.prop9;
        t0 = p.prop0; t1 = p.prop1; t2 = p.prop2; t3 = p.prop3; t4 = p.prop4; t5 = p.prop5; t6 = p.prop6; t7 = p.prop7; t8 = p.prop8; t9 = p.prop9;
        t0 = p.prop0; t1 = p.prop1; t2 = p.prop2; t3 = p.prop3; t4 = p.prop4; t5 = p.prop5; t6 = p.prop6; t7 = p.prop7; t8 = p.prop8; t9 = p.prop9;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("getProp_1a : " + elapsedTime);
    total += elapsedTime;
}

// get different properties from different objects, passed as parameters
function getProp_1b(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9)
{
    var t0, t1, t2, t3, t4, t5, t6, t7, t8, t9;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t0 = p0.prop0; t1 = p1.prop1; t2 = p2.prop2; t3 = p3.prop3; t4 = p4.prop4; t5 = p5.prop5; t6 = p6.prop6; t7 = p7.prop7; t8 = p8.prop8; t9 = p9.prop9;
        t0 = p0.prop0; t1 = p1.prop1; t2 = p2.prop2; t3 = p3.prop3; t4 = p4.prop4; t5 = p5.prop5; t6 = p6.prop6; t7 = p7.prop7; t8 = p8.prop8; t9 = p9.prop9;
        t0 = p0.prop0; t1 = p1.prop1; t2 = p2.prop2; t3 = p3.prop3; t4 = p4.prop4; t5 = p5.prop5; t6 = p6.prop6; t7 = p7.prop7; t8 = p8.prop8; t9 = p9.prop9;
        t0 = p0.prop0; t1 = p1.prop1; t2 = p2.prop2; t3 = p3.prop3; t4 = p4.prop4; t5 = p5.prop5; t6 = p6.prop6; t7 = p7.prop7; t8 = p8.prop8; t9 = p9.prop9;
        t0 = p0.prop0; t1 = p1.prop1; t2 = p2.prop2; t3 = p3.prop3; t4 = p4.prop4; t5 = p5.prop5; t6 = p6.prop6; t7 = p7.prop7; t8 = p8.prop8; t9 = p9.prop9;
        t0 = p0.prop0; t1 = p1.prop1; t2 = p2.prop2; t3 = p3.prop3; t4 = p4.prop4; t5 = p5.prop5; t6 = p6.prop6; t7 = p7.prop7; t8 = p8.prop8; t9 = p9.prop9;
        t0 = p0.prop0; t1 = p1.prop1; t2 = p2.prop2; t3 = p3.prop3; t4 = p4.prop4; t5 = p5.prop5; t6 = p6.prop6; t7 = p7.prop7; t8 = p8.prop8; t9 = p9.prop9;
        t0 = p0.prop0; t1 = p1.prop1; t2 = p2.prop2; t3 = p3.prop3; t4 = p4.prop4; t5 = p5.prop5; t6 = p6.prop6; t7 = p7.prop7; t8 = p8.prop8; t9 = p9.prop9;
        t0 = p0.prop0; t1 = p1.prop1; t2 = p2.prop2; t3 = p3.prop3; t4 = p4.prop4; t5 = p5.prop5; t6 = p6.prop6; t7 = p7.prop7; t8 = p8.prop8; t9 = p9.prop9;
        t0 = p0.prop0; t1 = p1.prop1; t2 = p2.prop2; t3 = p3.prop3; t4 = p4.prop4; t5 = p5.prop5; t6 = p6.prop6; t7 = p7.prop7; t8 = p8.prop8; t9 = p9.prop9;
        t0 = p0.prop0; t1 = p1.prop1; t2 = p2.prop2; t3 = p3.prop3; t4 = p4.prop4; t5 = p5.prop5; t6 = p6.prop6; t7 = p7.prop7; t8 = p8.prop8; t9 = p9.prop9;
        t0 = p0.prop0; t1 = p1.prop1; t2 = p2.prop2; t3 = p3.prop3; t4 = p4.prop4; t5 = p5.prop5; t6 = p6.prop6; t7 = p7.prop7; t8 = p8.prop8; t9 = p9.prop9;
        t0 = p0.prop0; t1 = p1.prop1; t2 = p2.prop2; t3 = p3.prop3; t4 = p4.prop4; t5 = p5.prop5; t6 = p6.prop6; t7 = p7.prop7; t8 = p8.prop8; t9 = p9.prop9;
        t0 = p0.prop0; t1 = p1.prop1; t2 = p2.prop2; t3 = p3.prop3; t4 = p4.prop4; t5 = p5.prop5; t6 = p6.prop6; t7 = p7.prop7; t8 = p8.prop8; t9 = p9.prop9;
        t0 = p0.prop0; t1 = p1.prop1; t2 = p2.prop2; t3 = p3.prop3; t4 = p4.prop4; t5 = p5.prop5; t6 = p6.prop6; t7 = p7.prop7; t8 = p8.prop8; t9 = p9.prop9;
        t0 = p0.prop0; t1 = p1.prop1; t2 = p2.prop2; t3 = p3.prop3; t4 = p4.prop4; t5 = p5.prop5; t6 = p6.prop6; t7 = p7.prop7; t8 = p8.prop8; t9 = p9.prop9;
        t0 = p0.prop0; t1 = p1.prop1; t2 = p2.prop2; t3 = p3.prop3; t4 = p4.prop4; t5 = p5.prop5; t6 = p6.prop6; t7 = p7.prop7; t8 = p8.prop8; t9 = p9.prop9;
        t0 = p0.prop0; t1 = p1.prop1; t2 = p2.prop2; t3 = p3.prop3; t4 = p4.prop4; t5 = p5.prop5; t6 = p6.prop6; t7 = p7.prop7; t8 = p8.prop8; t9 = p9.prop9;
        t0 = p0.prop0; t1 = p1.prop1; t2 = p2.prop2; t3 = p3.prop3; t4 = p4.prop4; t5 = p5.prop5; t6 = p6.prop6; t7 = p7.prop7; t8 = p8.prop8; t9 = p9.prop9;
        t0 = p0.prop0; t1 = p1.prop1; t2 = p2.prop2; t3 = p3.prop3; t4 = p4.prop4; t5 = p5.prop5; t6 = p6.prop6; t7 = p7.prop7; t8 = p8.prop8; t9 = p9.prop9;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("getProp_1b : " + elapsedTime);
    total += elapsedTime;
}

var gP = { };
gP.prop0 = { };
gP.prop1 = { };
gP.prop2 = { };
gP.prop3 = { };
gP.prop4 = { };
gP.prop5 = { };
gP.prop6 = { };
gP.prop7 = { };
gP.prop8 = { };
gP.prop9 = { };

var gP0 = gP;
var gP1 = gP;
var gP2 = gP;
var gP3 = gP;
var gP4 = gP;
var gP5 = gP;
var gP6 = gP;
var gP7 = gP;
var gP8 = gP;
var gP9 = gP;

// get the same property from the same object in script scope
function getProp_2()
{
    var t;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("getProp_2 : " + elapsedTime);
    total += elapsedTime;
}

// get different properties from the same object in script scope
function getProp_2a()
{
    var t0, t1, t2, t3, t4, t5, t6, t7, t8, t9;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("getProp_2a : " + elapsedTime);
    total += elapsedTime;
}


// get different properties from different objects in script scope
function getProp_2b()
{
    var t0, t1, t2, t3, t4, t5, t6, t7, t8, t9;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("getProp_2b : " + elapsedTime);
    total += elapsedTime;
}

// get the same property from the this object
function getProp_3()
{
    var t;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1;
        t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1;
        t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1;
        t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1;
        t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1;
        t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1;
        t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1;
        t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1;
        t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1;
        t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1;
        t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1;
        t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1;
        t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1;
        t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1;
        t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1;
        t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1;
        t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1;
        t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1;
        t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1;
        t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1; t = this.prop1;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("getProp_3 : " + elapsedTime);
    total += elapsedTime;
}

// get different properties from the this object
function getProp_3a()
{
    //for(var list in this.prop1)
      //print(list);
    //return;

    var t0, t1, t2, t3, t4, t5, t6, t7, t8, t9;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t0 = this.prop0; t1 = this.prop1; t2 = this.prop2; t3 = this.prop3; t4 = this.prop4; t5 = this.prop5; t6 = this.prop6; t7 = this.prop7; t8 = this.prop8; t9 = this.prop9;
        t0 = this.prop0; t1 = this.prop1; t2 = this.prop2; t3 = this.prop3; t4 = this.prop4; t5 = this.prop5; t6 = this.prop6; t7 = this.prop7; t8 = this.prop8; t9 = this.prop9;
        t0 = this.prop0; t1 = this.prop1; t2 = this.prop2; t3 = this.prop3; t4 = this.prop4; t5 = this.prop5; t6 = this.prop6; t7 = this.prop7; t8 = this.prop8; t9 = this.prop9;
        t0 = this.prop0; t1 = this.prop1; t2 = this.prop2; t3 = this.prop3; t4 = this.prop4; t5 = this.prop5; t6 = this.prop6; t7 = this.prop7; t8 = this.prop8; t9 = this.prop9;
        t0 = this.prop0; t1 = this.prop1; t2 = this.prop2; t3 = this.prop3; t4 = this.prop4; t5 = this.prop5; t6 = this.prop6; t7 = this.prop7; t8 = this.prop8; t9 = this.prop9;
        t0 = this.prop0; t1 = this.prop1; t2 = this.prop2; t3 = this.prop3; t4 = this.prop4; t5 = this.prop5; t6 = this.prop6; t7 = this.prop7; t8 = this.prop8; t9 = this.prop9;
        t0 = this.prop0; t1 = this.prop1; t2 = this.prop2; t3 = this.prop3; t4 = this.prop4; t5 = this.prop5; t6 = this.prop6; t7 = this.prop7; t8 = this.prop8; t9 = this.prop9;
        t0 = this.prop0; t1 = this.prop1; t2 = this.prop2; t3 = this.prop3; t4 = this.prop4; t5 = this.prop5; t6 = this.prop6; t7 = this.prop7; t8 = this.prop8; t9 = this.prop9;
        t0 = this.prop0; t1 = this.prop1; t2 = this.prop2; t3 = this.prop3; t4 = this.prop4; t5 = this.prop5; t6 = this.prop6; t7 = this.prop7; t8 = this.prop8; t9 = this.prop9;
        t0 = this.prop0; t1 = this.prop1; t2 = this.prop2; t3 = this.prop3; t4 = this.prop4; t5 = this.prop5; t6 = this.prop6; t7 = this.prop7; t8 = this.prop8; t9 = this.prop9;
        t0 = this.prop0; t1 = this.prop1; t2 = this.prop2; t3 = this.prop3; t4 = this.prop4; t5 = this.prop5; t6 = this.prop6; t7 = this.prop7; t8 = this.prop8; t9 = this.prop9;
        t0 = this.prop0; t1 = this.prop1; t2 = this.prop2; t3 = this.prop3; t4 = this.prop4; t5 = this.prop5; t6 = this.prop6; t7 = this.prop7; t8 = this.prop8; t9 = this.prop9;
        t0 = this.prop0; t1 = this.prop1; t2 = this.prop2; t3 = this.prop3; t4 = this.prop4; t5 = this.prop5; t6 = this.prop6; t7 = this.prop7; t8 = this.prop8; t9 = this.prop9;
        t0 = this.prop0; t1 = this.prop1; t2 = this.prop2; t3 = this.prop3; t4 = this.prop4; t5 = this.prop5; t6 = this.prop6; t7 = this.prop7; t8 = this.prop8; t9 = this.prop9;
        t0 = this.prop0; t1 = this.prop1; t2 = this.prop2; t3 = this.prop3; t4 = this.prop4; t5 = this.prop5; t6 = this.prop6; t7 = this.prop7; t8 = this.prop8; t9 = this.prop9;
        t0 = this.prop0; t1 = this.prop1; t2 = this.prop2; t3 = this.prop3; t4 = this.prop4; t5 = this.prop5; t6 = this.prop6; t7 = this.prop7; t8 = this.prop8; t9 = this.prop9;
        t0 = this.prop0; t1 = this.prop1; t2 = this.prop2; t3 = this.prop3; t4 = this.prop4; t5 = this.prop5; t6 = this.prop6; t7 = this.prop7; t8 = this.prop8; t9 = this.prop9;
        t0 = this.prop0; t1 = this.prop1; t2 = this.prop2; t3 = this.prop3; t4 = this.prop4; t5 = this.prop5; t6 = this.prop6; t7 = this.prop7; t8 = this.prop8; t9 = this.prop9;
        t0 = this.prop0; t1 = this.prop1; t2 = this.prop2; t3 = this.prop3; t4 = this.prop4; t5 = this.prop5; t6 = this.prop6; t7 = this.prop7; t8 = this.prop8; t9 = this.prop9;
        t0 = this.prop0; t1 = this.prop1; t2 = this.prop2; t3 = this.prop3; t4 = this.prop4; t5 = this.prop5; t6 = this.prop6; t7 = this.prop7; t8 = this.prop8; t9 = this.prop9;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("getProp_3a : " + elapsedTime);
    total += elapsedTime;
}

// get the same property from the same property of a var
function deepGet_1()
{
    var startTime = new Date();

    var v = { };
    v.prop0 = { };
    v.prop0.prop0 = { };

    var t;

    for (var i = 0; i < 5000; i++) {
        t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0;
        t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0;
        t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0;
        t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0;
        t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0;
        t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0;
        t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0;
        t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0;
        t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0;
        t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0;
        t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0;
        t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0;
        t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0;
        t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0;
        t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0;
        t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0;
        t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0;
        t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0;
        t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0;
        t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0; t = v.prop0.prop0;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("deepGet_1 : " + elapsedTime);
    total += elapsedTime;
}

// get a different property from a different property of a var
function deepGet_2()
{
    var startTime = new Date();

    var v = { };
    v.prop0 = { };
    v.prop0.prop0 = { };
    v.prop1 = { };
    v.prop1.prop1 = { };
    v.prop2 = { };
    v.prop2.prop2 = { };
    v.prop3 = { };
    v.prop3.prop3 = { };
    v.prop4 = { };
    v.prop4.prop4 = { };
    v.prop5 = { };
    v.prop5.prop5 = { };
    v.prop6 = { };
    v.prop6.prop6 = { };
    v.prop7 = { };
    v.prop7.prop7 = { };
    v.prop8 = { };
    v.prop8.prop8 = { };
    v.prop9 = { };
    v.prop9.prop9 = { };

    var t;

    for (var i = 0; i < 5000; i++) {
        t = v.prop0.prop0; t = v.prop1.prop1; t = v.prop2.prop2; t = v.prop3.prop3; t = v.prop4.prop4; t = v.prop5.prop5; t = v.prop6.prop6; t = v.prop7.prop7; t = v.prop8.prop8; t = v.prop9.prop9;
        t = v.prop0.prop0; t = v.prop1.prop1; t = v.prop2.prop2; t = v.prop3.prop3; t = v.prop4.prop4; t = v.prop5.prop5; t = v.prop6.prop6; t = v.prop7.prop7; t = v.prop8.prop8; t = v.prop9.prop9;
        t = v.prop0.prop0; t = v.prop1.prop1; t = v.prop2.prop2; t = v.prop3.prop3; t = v.prop4.prop4; t = v.prop5.prop5; t = v.prop6.prop6; t = v.prop7.prop7; t = v.prop8.prop8; t = v.prop9.prop9;
        t = v.prop0.prop0; t = v.prop1.prop1; t = v.prop2.prop2; t = v.prop3.prop3; t = v.prop4.prop4; t = v.prop5.prop5; t = v.prop6.prop6; t = v.prop7.prop7; t = v.prop8.prop8; t = v.prop9.prop9;
        t = v.prop0.prop0; t = v.prop1.prop1; t = v.prop2.prop2; t = v.prop3.prop3; t = v.prop4.prop4; t = v.prop5.prop5; t = v.prop6.prop6; t = v.prop7.prop7; t = v.prop8.prop8; t = v.prop9.prop9;
        t = v.prop0.prop0; t = v.prop1.prop1; t = v.prop2.prop2; t = v.prop3.prop3; t = v.prop4.prop4; t = v.prop5.prop5; t = v.prop6.prop6; t = v.prop7.prop7; t = v.prop8.prop8; t = v.prop9.prop9;
        t = v.prop0.prop0; t = v.prop1.prop1; t = v.prop2.prop2; t = v.prop3.prop3; t = v.prop4.prop4; t = v.prop5.prop5; t = v.prop6.prop6; t = v.prop7.prop7; t = v.prop8.prop8; t = v.prop9.prop9;
        t = v.prop0.prop0; t = v.prop1.prop1; t = v.prop2.prop2; t = v.prop3.prop3; t = v.prop4.prop4; t = v.prop5.prop5; t = v.prop6.prop6; t = v.prop7.prop7; t = v.prop8.prop8; t = v.prop9.prop9;
        t = v.prop0.prop0; t = v.prop1.prop1; t = v.prop2.prop2; t = v.prop3.prop3; t = v.prop4.prop4; t = v.prop5.prop5; t = v.prop6.prop6; t = v.prop7.prop7; t = v.prop8.prop8; t = v.prop9.prop9;
        t = v.prop0.prop0; t = v.prop1.prop1; t = v.prop2.prop2; t = v.prop3.prop3; t = v.prop4.prop4; t = v.prop5.prop5; t = v.prop6.prop6; t = v.prop7.prop7; t = v.prop8.prop8; t = v.prop9.prop9;
        t = v.prop0.prop0; t = v.prop1.prop1; t = v.prop2.prop2; t = v.prop3.prop3; t = v.prop4.prop4; t = v.prop5.prop5; t = v.prop6.prop6; t = v.prop7.prop7; t = v.prop8.prop8; t = v.prop9.prop9;
        t = v.prop0.prop0; t = v.prop1.prop1; t = v.prop2.prop2; t = v.prop3.prop3; t = v.prop4.prop4; t = v.prop5.prop5; t = v.prop6.prop6; t = v.prop7.prop7; t = v.prop8.prop8; t = v.prop9.prop9;
        t = v.prop0.prop0; t = v.prop1.prop1; t = v.prop2.prop2; t = v.prop3.prop3; t = v.prop4.prop4; t = v.prop5.prop5; t = v.prop6.prop6; t = v.prop7.prop7; t = v.prop8.prop8; t = v.prop9.prop9;
        t = v.prop0.prop0; t = v.prop1.prop1; t = v.prop2.prop2; t = v.prop3.prop3; t = v.prop4.prop4; t = v.prop5.prop5; t = v.prop6.prop6; t = v.prop7.prop7; t = v.prop8.prop8; t = v.prop9.prop9;
        t = v.prop0.prop0; t = v.prop1.prop1; t = v.prop2.prop2; t = v.prop3.prop3; t = v.prop4.prop4; t = v.prop5.prop5; t = v.prop6.prop6; t = v.prop7.prop7; t = v.prop8.prop8; t = v.prop9.prop9;
        t = v.prop0.prop0; t = v.prop1.prop1; t = v.prop2.prop2; t = v.prop3.prop3; t = v.prop4.prop4; t = v.prop5.prop5; t = v.prop6.prop6; t = v.prop7.prop7; t = v.prop8.prop8; t = v.prop9.prop9;
        t = v.prop0.prop0; t = v.prop1.prop1; t = v.prop2.prop2; t = v.prop3.prop3; t = v.prop4.prop4; t = v.prop5.prop5; t = v.prop6.prop6; t = v.prop7.prop7; t = v.prop8.prop8; t = v.prop9.prop9;
        t = v.prop0.prop0; t = v.prop1.prop1; t = v.prop2.prop2; t = v.prop3.prop3; t = v.prop4.prop4; t = v.prop5.prop5; t = v.prop6.prop6; t = v.prop7.prop7; t = v.prop8.prop8; t = v.prop9.prop9;
        t = v.prop0.prop0; t = v.prop1.prop1; t = v.prop2.prop2; t = v.prop3.prop3; t = v.prop4.prop4; t = v.prop5.prop5; t = v.prop6.prop6; t = v.prop7.prop7; t = v.prop8.prop8; t = v.prop9.prop9;
        t = v.prop0.prop0; t = v.prop1.prop1; t = v.prop2.prop2; t = v.prop3.prop3; t = v.prop4.prop4; t = v.prop5.prop5; t = v.prop6.prop6; t = v.prop7.prop7; t = v.prop8.prop8; t = v.prop9.prop9;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("deepGet_2 : " + elapsedTime);
    total += elapsedTime;
}


getProp_1(gP);
getProp_1a(gP);
getProp_1b(gP0, gP1, gP2, gP3, gP4, gP5, gP6, gP7, gP8, gP9);
getProp_2();
getProp_2a();
getProp_2b();
var tP = { };
tP.doit = getProp_3;
tP.doit2 = getProp_3a;
tP.prop0 = { };
tP.prop1 = { };
tP.prop2 = { };
tP.prop3 = { };
tP.prop4 = { };
tP.prop5 = { };
tP.prop6 = { };
tP.prop7 = { };
tP.prop8 = { };
tP.prop9 = { };
tP.doit();
tP.doit2();

deepGet_1();
deepGet_2();

// get the same property from the same object in script scope, at script scope

    var t;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
        t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1; t = gP.prop1;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("getProp_S : " + elapsedTime);
    total += elapsedTime;

// get different properties from the same object in script scope, at script scope

    var t0, t1, t2, t3, t4, t5, t6, t7, t8, t9;
    startTime = new Date();

    for (i = 0; i < 5000; i++) {
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
        t0 = gP.prop0; t1 = gP.prop1; t2 = gP.prop2; t3 = gP.prop3; t4 = gP.prop4; t5 = gP.prop5; t6 = gP.prop6; t7 = gP.prop7; t8 = gP.prop8; t9 = gP.prop9;
    }
    elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("getProp_Sa : " + elapsedTime);
    total += elapsedTime;

// get different properties from different objects in script scope, at script scope

    //var t1, t2, t3, t4, t5, t6, t7, t8, t9;
    startTime = new Date();

    for (i = 0; i < 5000; i++) {
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
        t0 = gP0.prop0; t1 = gP1.prop1; t2 = gP2.prop2; t3 = gP3.prop3; t4 = gP4.prop4; t5 = gP5.prop5; t6 = gP6.prop6; t7 = gP7.prop7; t8 = gP8.prop8; t9 = gP9.prop9;
    }
    elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("getProp_Sb : " + elapsedTime);
    total += elapsedTime;
