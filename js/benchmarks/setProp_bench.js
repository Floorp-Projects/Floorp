var total="";
print("setProp_bench");

// set the same property into the same object, passed as a parameter
function setProp_1(p)
{
    var t = { };
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t;
        p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t;
        p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t;
        p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t;
        p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t;
        p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t;
        p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t;
        p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t;
        p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t;
        p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t;
        p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t;
        p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t;
        p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t;
        p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t;
        p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t;
        p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t;
        p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t;
        p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t;
        p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t;
        p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t; p.prop1 = t;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("setProp_1 : " + elapsedTime);
    total += elapsedTime;
}

// set different properties into the same object, passed as a parameter
function setProp_1a(p)
{
    var t0 = { };
    var t1 = { };
    var t2 = { };
    var t3 = { };
    var t4 = { };
    var t5 = { };
    var t6 = { };
    var t7 = { };
    var t8 = { };
    var t9 = { };
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        p.prop0 = t0; p.prop1 = t1; p.prop2 = t2; p.prop3 = t3; p.prop4 = t4; p.prop5 = t5; p.prop6 = t6; p.prop7 = t7; p.prop8 = t8; p.prop9 = t9;
        p.prop0 = t0; p.prop1 = t1; p.prop2 = t2; p.prop3 = t3; p.prop4 = t4; p.prop5 = t5; p.prop6 = t6; p.prop7 = t7; p.prop8 = t8; p.prop9 = t9;
        p.prop0 = t0; p.prop1 = t1; p.prop2 = t2; p.prop3 = t3; p.prop4 = t4; p.prop5 = t5; p.prop6 = t6; p.prop7 = t7; p.prop8 = t8; p.prop9 = t9;
        p.prop0 = t0; p.prop1 = t1; p.prop2 = t2; p.prop3 = t3; p.prop4 = t4; p.prop5 = t5; p.prop6 = t6; p.prop7 = t7; p.prop8 = t8; p.prop9 = t9;
        p.prop0 = t0; p.prop1 = t1; p.prop2 = t2; p.prop3 = t3; p.prop4 = t4; p.prop5 = t5; p.prop6 = t6; p.prop7 = t7; p.prop8 = t8; p.prop9 = t9;
        p.prop0 = t0; p.prop1 = t1; p.prop2 = t2; p.prop3 = t3; p.prop4 = t4; p.prop5 = t5; p.prop6 = t6; p.prop7 = t7; p.prop8 = t8; p.prop9 = t9;
        p.prop0 = t0; p.prop1 = t1; p.prop2 = t2; p.prop3 = t3; p.prop4 = t4; p.prop5 = t5; p.prop6 = t6; p.prop7 = t7; p.prop8 = t8; p.prop9 = t9;
        p.prop0 = t0; p.prop1 = t1; p.prop2 = t2; p.prop3 = t3; p.prop4 = t4; p.prop5 = t5; p.prop6 = t6; p.prop7 = t7; p.prop8 = t8; p.prop9 = t9;
        p.prop0 = t0; p.prop1 = t1; p.prop2 = t2; p.prop3 = t3; p.prop4 = t4; p.prop5 = t5; p.prop6 = t6; p.prop7 = t7; p.prop8 = t8; p.prop9 = t9;
        p.prop0 = t0; p.prop1 = t1; p.prop2 = t2; p.prop3 = t3; p.prop4 = t4; p.prop5 = t5; p.prop6 = t6; p.prop7 = t7; p.prop8 = t8; p.prop9 = t9;
        p.prop0 = t0; p.prop1 = t1; p.prop2 = t2; p.prop3 = t3; p.prop4 = t4; p.prop5 = t5; p.prop6 = t6; p.prop7 = t7; p.prop8 = t8; p.prop9 = t9;
        p.prop0 = t0; p.prop1 = t1; p.prop2 = t2; p.prop3 = t3; p.prop4 = t4; p.prop5 = t5; p.prop6 = t6; p.prop7 = t7; p.prop8 = t8; p.prop9 = t9;
        p.prop0 = t0; p.prop1 = t1; p.prop2 = t2; p.prop3 = t3; p.prop4 = t4; p.prop5 = t5; p.prop6 = t6; p.prop7 = t7; p.prop8 = t8; p.prop9 = t9;
        p.prop0 = t0; p.prop1 = t1; p.prop2 = t2; p.prop3 = t3; p.prop4 = t4; p.prop5 = t5; p.prop6 = t6; p.prop7 = t7; p.prop8 = t8; p.prop9 = t9;
        p.prop0 = t0; p.prop1 = t1; p.prop2 = t2; p.prop3 = t3; p.prop4 = t4; p.prop5 = t5; p.prop6 = t6; p.prop7 = t7; p.prop8 = t8; p.prop9 = t9;
        p.prop0 = t0; p.prop1 = t1; p.prop2 = t2; p.prop3 = t3; p.prop4 = t4; p.prop5 = t5; p.prop6 = t6; p.prop7 = t7; p.prop8 = t8; p.prop9 = t9;
        p.prop0 = t0; p.prop1 = t1; p.prop2 = t2; p.prop3 = t3; p.prop4 = t4; p.prop5 = t5; p.prop6 = t6; p.prop7 = t7; p.prop8 = t8; p.prop9 = t9;
        p.prop0 = t0; p.prop1 = t1; p.prop2 = t2; p.prop3 = t3; p.prop4 = t4; p.prop5 = t5; p.prop6 = t6; p.prop7 = t7; p.prop8 = t8; p.prop9 = t9;
        p.prop0 = t0; p.prop1 = t1; p.prop2 = t2; p.prop3 = t3; p.prop4 = t4; p.prop5 = t5; p.prop6 = t6; p.prop7 = t7; p.prop8 = t8; p.prop9 = t9;
        p.prop0 = t0; p.prop1 = t1; p.prop2 = t2; p.prop3 = t3; p.prop4 = t4; p.prop5 = t5; p.prop6 = t6; p.prop7 = t7; p.prop8 = t8; p.prop9 = t9;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("setProp_1a : " + elapsedTime);
    total += elapsedTime;
}

// set different properties into different objects, passed as parameters
function setProp_1b(p0, p1, p2, p3, p4, p5, p6, p7, p8, p9)
{
    var t0, t1, t2, t3, t4, t5, t6, t7, t8, t9;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        p0.prop0 = t0; p1.prop1 = t1; p2.prop2 = t2; p3.prop3 = t3; p4.prop4 = t4; p5.prop5 = t5; p6.prop6 = t6; p7.prop7 = t7; p8.prop8 = t8; p9.prop9 = t9;
        p0.prop0 = t0; p1.prop1 = t1; p2.prop2 = t2; p3.prop3 = t3; p4.prop4 = t4; p5.prop5 = t5; p6.prop6 = t6; p7.prop7 = t7; p8.prop8 = t8; p9.prop9 = t9;
        p0.prop0 = t0; p1.prop1 = t1; p2.prop2 = t2; p3.prop3 = t3; p4.prop4 = t4; p5.prop5 = t5; p6.prop6 = t6; p7.prop7 = t7; p8.prop8 = t8; p9.prop9 = t9;
        p0.prop0 = t0; p1.prop1 = t1; p2.prop2 = t2; p3.prop3 = t3; p4.prop4 = t4; p5.prop5 = t5; p6.prop6 = t6; p7.prop7 = t7; p8.prop8 = t8; p9.prop9 = t9;
        p0.prop0 = t0; p1.prop1 = t1; p2.prop2 = t2; p3.prop3 = t3; p4.prop4 = t4; p5.prop5 = t5; p6.prop6 = t6; p7.prop7 = t7; p8.prop8 = t8; p9.prop9 = t9;
        p0.prop0 = t0; p1.prop1 = t1; p2.prop2 = t2; p3.prop3 = t3; p4.prop4 = t4; p5.prop5 = t5; p6.prop6 = t6; p7.prop7 = t7; p8.prop8 = t8; p9.prop9 = t9;
        p0.prop0 = t0; p1.prop1 = t1; p2.prop2 = t2; p3.prop3 = t3; p4.prop4 = t4; p5.prop5 = t5; p6.prop6 = t6; p7.prop7 = t7; p8.prop8 = t8; p9.prop9 = t9;
        p0.prop0 = t0; p1.prop1 = t1; p2.prop2 = t2; p3.prop3 = t3; p4.prop4 = t4; p5.prop5 = t5; p6.prop6 = t6; p7.prop7 = t7; p8.prop8 = t8; p9.prop9 = t9;
        p0.prop0 = t0; p1.prop1 = t1; p2.prop2 = t2; p3.prop3 = t3; p4.prop4 = t4; p5.prop5 = t5; p6.prop6 = t6; p7.prop7 = t7; p8.prop8 = t8; p9.prop9 = t9;
        p0.prop0 = t0; p1.prop1 = t1; p2.prop2 = t2; p3.prop3 = t3; p4.prop4 = t4; p5.prop5 = t5; p6.prop6 = t6; p7.prop7 = t7; p8.prop8 = t8; p9.prop9 = t9;
        p0.prop0 = t0; p1.prop1 = t1; p2.prop2 = t2; p3.prop3 = t3; p4.prop4 = t4; p5.prop5 = t5; p6.prop6 = t6; p7.prop7 = t7; p8.prop8 = t8; p9.prop9 = t9;
        p0.prop0 = t0; p1.prop1 = t1; p2.prop2 = t2; p3.prop3 = t3; p4.prop4 = t4; p5.prop5 = t5; p6.prop6 = t6; p7.prop7 = t7; p8.prop8 = t8; p9.prop9 = t9;
        p0.prop0 = t0; p1.prop1 = t1; p2.prop2 = t2; p3.prop3 = t3; p4.prop4 = t4; p5.prop5 = t5; p6.prop6 = t6; p7.prop7 = t7; p8.prop8 = t8; p9.prop9 = t9;
        p0.prop0 = t0; p1.prop1 = t1; p2.prop2 = t2; p3.prop3 = t3; p4.prop4 = t4; p5.prop5 = t5; p6.prop6 = t6; p7.prop7 = t7; p8.prop8 = t8; p9.prop9 = t9;
        p0.prop0 = t0; p1.prop1 = t1; p2.prop2 = t2; p3.prop3 = t3; p4.prop4 = t4; p5.prop5 = t5; p6.prop6 = t6; p7.prop7 = t7; p8.prop8 = t8; p9.prop9 = t9;
        p0.prop0 = t0; p1.prop1 = t1; p2.prop2 = t2; p3.prop3 = t3; p4.prop4 = t4; p5.prop5 = t5; p6.prop6 = t6; p7.prop7 = t7; p8.prop8 = t8; p9.prop9 = t9;
        p0.prop0 = t0; p1.prop1 = t1; p2.prop2 = t2; p3.prop3 = t3; p4.prop4 = t4; p5.prop5 = t5; p6.prop6 = t6; p7.prop7 = t7; p8.prop8 = t8; p9.prop9 = t9;
        p0.prop0 = t0; p1.prop1 = t1; p2.prop2 = t2; p3.prop3 = t3; p4.prop4 = t4; p5.prop5 = t5; p6.prop6 = t6; p7.prop7 = t7; p8.prop8 = t8; p9.prop9 = t9;
        p0.prop0 = t0; p1.prop1 = t1; p2.prop2 = t2; p3.prop3 = t3; p4.prop4 = t4; p5.prop5 = t5; p6.prop6 = t6; p7.prop7 = t7; p8.prop8 = t8; p9.prop9 = t9;
        p0.prop0 = t0; p1.prop1 = t1; p2.prop2 = t2; p3.prop3 = t3; p4.prop4 = t4; p5.prop5 = t5; p6.prop6 = t6; p7.prop7 = t7; p8.prop8 = t8; p9.prop9 = t9;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("setProp_1b : " + elapsedTime);
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

// set the same property into the same object in script scope
function setProp_2()
{
    var t = { };
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("setProp_2 : " + elapsedTime);
    total += elapsedTime;
}

// set different properties into the same object in script scope
function setProp_2a()
{
    var t0 = { };
    var t1 = { };
    var t2 = { };
    var t3 = { };
    var t4 = { };
    var t5 = { };
    var t6 = { };
    var t7 = { };
    var t8 = { };
    var t9 = { };
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("setProp_2a : " + elapsedTime);
    total += elapsedTime;
}


// set different properties into different objects in script scope
function setProp_2b()
{
    var t0 = { };
    var t1 = { };
    var t2 = { };
    var t3 = { };
    var t4 = { };
    var t5 = { };
    var t6 = { };
    var t7 = { };
    var t8 = { };
    var t9 = { };
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("setProp_2b : " + elapsedTime);
    total += elapsedTime;
}

// set the same property into the this object
function setProp_3()
{
    var t = { };
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t;
        this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t;
        this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t;
        this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t;
        this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t;
        this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t;
        this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t;
        this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t;
        this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t;
        this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t;
        this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t;
        this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t;
        this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t;
        this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t;
        this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t;
        this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t;
        this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t;
        this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t;
        this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t;
        this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t; this.prop1 = t;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("setProp_3 : " + elapsedTime);
    total += elapsedTime;
}

// set different properties from the this object
function setProp_3a()
{
    var t0 = { };
    var t1 = { };
    var t2 = { };
    var t3 = { };
    var t4 = { };
    var t5 = { };
    var t6 = { };
    var t7 = { };
    var t8 = { };
    var t9 = { };
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        this.prop0 = t0; this.prop1 = t1; this.prop2 = t2; this.prop3 = t3; this.prop4 = t4; this.prop5 = t5; this.prop6 = t6; this.prop7 = t7; this.prop8 = t8; this.prop9 = t9;
        this.prop0 = t0; this.prop1 = t1; this.prop2 = t2; this.prop3 = t3; this.prop4 = t4; this.prop5 = t5; this.prop6 = t6; this.prop7 = t7; this.prop8 = t8; this.prop9 = t9;
        this.prop0 = t0; this.prop1 = t1; this.prop2 = t2; this.prop3 = t3; this.prop4 = t4; this.prop5 = t5; this.prop6 = t6; this.prop7 = t7; this.prop8 = t8; this.prop9 = t9;
        this.prop0 = t0; this.prop1 = t1; this.prop2 = t2; this.prop3 = t3; this.prop4 = t4; this.prop5 = t5; this.prop6 = t6; this.prop7 = t7; this.prop8 = t8; this.prop9 = t9;
        this.prop0 = t0; this.prop1 = t1; this.prop2 = t2; this.prop3 = t3; this.prop4 = t4; this.prop5 = t5; this.prop6 = t6; this.prop7 = t7; this.prop8 = t8; this.prop9 = t9;
        this.prop0 = t0; this.prop1 = t1; this.prop2 = t2; this.prop3 = t3; this.prop4 = t4; this.prop5 = t5; this.prop6 = t6; this.prop7 = t7; this.prop8 = t8; this.prop9 = t9;
        this.prop0 = t0; this.prop1 = t1; this.prop2 = t2; this.prop3 = t3; this.prop4 = t4; this.prop5 = t5; this.prop6 = t6; this.prop7 = t7; this.prop8 = t8; this.prop9 = t9;
        this.prop0 = t0; this.prop1 = t1; this.prop2 = t2; this.prop3 = t3; this.prop4 = t4; this.prop5 = t5; this.prop6 = t6; this.prop7 = t7; this.prop8 = t8; this.prop9 = t9;
        this.prop0 = t0; this.prop1 = t1; this.prop2 = t2; this.prop3 = t3; this.prop4 = t4; this.prop5 = t5; this.prop6 = t6; this.prop7 = t7; this.prop8 = t8; this.prop9 = t9;
        this.prop0 = t0; this.prop1 = t1; this.prop2 = t2; this.prop3 = t3; this.prop4 = t4; this.prop5 = t5; this.prop6 = t6; this.prop7 = t7; this.prop8 = t8; this.prop9 = t9;
        this.prop0 = t0; this.prop1 = t1; this.prop2 = t2; this.prop3 = t3; this.prop4 = t4; this.prop5 = t5; this.prop6 = t6; this.prop7 = t7; this.prop8 = t8; this.prop9 = t9;
        this.prop0 = t0; this.prop1 = t1; this.prop2 = t2; this.prop3 = t3; this.prop4 = t4; this.prop5 = t5; this.prop6 = t6; this.prop7 = t7; this.prop8 = t8; this.prop9 = t9;
        this.prop0 = t0; this.prop1 = t1; this.prop2 = t2; this.prop3 = t3; this.prop4 = t4; this.prop5 = t5; this.prop6 = t6; this.prop7 = t7; this.prop8 = t8; this.prop9 = t9;
        this.prop0 = t0; this.prop1 = t1; this.prop2 = t2; this.prop3 = t3; this.prop4 = t4; this.prop5 = t5; this.prop6 = t6; this.prop7 = t7; this.prop8 = t8; this.prop9 = t9;
        this.prop0 = t0; this.prop1 = t1; this.prop2 = t2; this.prop3 = t3; this.prop4 = t4; this.prop5 = t5; this.prop6 = t6; this.prop7 = t7; this.prop8 = t8; this.prop9 = t9;
        this.prop0 = t0; this.prop1 = t1; this.prop2 = t2; this.prop3 = t3; this.prop4 = t4; this.prop5 = t5; this.prop6 = t6; this.prop7 = t7; this.prop8 = t8; this.prop9 = t9;
        this.prop0 = t0; this.prop1 = t1; this.prop2 = t2; this.prop3 = t3; this.prop4 = t4; this.prop5 = t5; this.prop6 = t6; this.prop7 = t7; this.prop8 = t8; this.prop9 = t9;
        this.prop0 = t0; this.prop1 = t1; this.prop2 = t2; this.prop3 = t3; this.prop4 = t4; this.prop5 = t5; this.prop6 = t6; this.prop7 = t7; this.prop8 = t8; this.prop9 = t9;
        this.prop0 = t0; this.prop1 = t1; this.prop2 = t2; this.prop3 = t3; this.prop4 = t4; this.prop5 = t5; this.prop6 = t6; this.prop7 = t7; this.prop8 = t8; this.prop9 = t9;
        this.prop0 = t0; this.prop1 = t1; this.prop2 = t2; this.prop3 = t3; this.prop4 = t4; this.prop5 = t5; this.prop6 = t6; this.prop7 = t7; this.prop8 = t8; this.prop9 = t9;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("setProp_3a : " + elapsedTime);
    total += elapsedTime;
}

// set the same property into the same property of a var
function deepSet_1()
{
    var startTime = new Date();

    var v = { };
    v.prop0 = { };
    v.prop0.prop0 = { };

    var t = { };

    for (var i = 0; i < 5000; i++) {
        v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t;
        v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t;
        v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t;
        v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t;
        v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t;
        v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t;
        v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t;
        v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t;
        v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t;
        v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t;
        v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t;
        v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t;
        v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t;
        v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t;
        v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t;
        v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t;
        v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t;
        v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t;
        v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t;
        v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t; v.prop0.prop0 = t;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("deepSet_1 : " + elapsedTime);
    total += elapsedTime;
}

// set a different property into a different property of a var
function deepSet_2()
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
        v.prop0.prop0 = t; v.prop1.prop1 = t; v.prop2.prop2 = t; v.prop3.prop3 = t; v.prop4.prop4 = t; v.prop5.prop5 = t; v.prop6.prop6 = t; v.prop7.prop7 = t; v.prop8.prop8 = t; v.prop9.prop9 = t;
        v.prop0.prop0 = t; v.prop1.prop1 = t; v.prop2.prop2 = t; v.prop3.prop3 = t; v.prop4.prop4 = t; v.prop5.prop5 = t; v.prop6.prop6 = t; v.prop7.prop7 = t; v.prop8.prop8 = t; v.prop9.prop9 = t;
        v.prop0.prop0 = t; v.prop1.prop1 = t; v.prop2.prop2 = t; v.prop3.prop3 = t; v.prop4.prop4 = t; v.prop5.prop5 = t; v.prop6.prop6 = t; v.prop7.prop7 = t; v.prop8.prop8 = t; v.prop9.prop9 = t;
        v.prop0.prop0 = t; v.prop1.prop1 = t; v.prop2.prop2 = t; v.prop3.prop3 = t; v.prop4.prop4 = t; v.prop5.prop5 = t; v.prop6.prop6 = t; v.prop7.prop7 = t; v.prop8.prop8 = t; v.prop9.prop9 = t;
        v.prop0.prop0 = t; v.prop1.prop1 = t; v.prop2.prop2 = t; v.prop3.prop3 = t; v.prop4.prop4 = t; v.prop5.prop5 = t; v.prop6.prop6 = t; v.prop7.prop7 = t; v.prop8.prop8 = t; v.prop9.prop9 = t;
        v.prop0.prop0 = t; v.prop1.prop1 = t; v.prop2.prop2 = t; v.prop3.prop3 = t; v.prop4.prop4 = t; v.prop5.prop5 = t; v.prop6.prop6 = t; v.prop7.prop7 = t; v.prop8.prop8 = t; v.prop9.prop9 = t;
        v.prop0.prop0 = t; v.prop1.prop1 = t; v.prop2.prop2 = t; v.prop3.prop3 = t; v.prop4.prop4 = t; v.prop5.prop5 = t; v.prop6.prop6 = t; v.prop7.prop7 = t; v.prop8.prop8 = t; v.prop9.prop9 = t;
        v.prop0.prop0 = t; v.prop1.prop1 = t; v.prop2.prop2 = t; v.prop3.prop3 = t; v.prop4.prop4 = t; v.prop5.prop5 = t; v.prop6.prop6 = t; v.prop7.prop7 = t; v.prop8.prop8 = t; v.prop9.prop9 = t;
        v.prop0.prop0 = t; v.prop1.prop1 = t; v.prop2.prop2 = t; v.prop3.prop3 = t; v.prop4.prop4 = t; v.prop5.prop5 = t; v.prop6.prop6 = t; v.prop7.prop7 = t; v.prop8.prop8 = t; v.prop9.prop9 = t;
        v.prop0.prop0 = t; v.prop1.prop1 = t; v.prop2.prop2 = t; v.prop3.prop3 = t; v.prop4.prop4 = t; v.prop5.prop5 = t; v.prop6.prop6 = t; v.prop7.prop7 = t; v.prop8.prop8 = t; v.prop9.prop9 = t;
        v.prop0.prop0 = t; v.prop1.prop1 = t; v.prop2.prop2 = t; v.prop3.prop3 = t; v.prop4.prop4 = t; v.prop5.prop5 = t; v.prop6.prop6 = t; v.prop7.prop7 = t; v.prop8.prop8 = t; v.prop9.prop9 = t;
        v.prop0.prop0 = t; v.prop1.prop1 = t; v.prop2.prop2 = t; v.prop3.prop3 = t; v.prop4.prop4 = t; v.prop5.prop5 = t; v.prop6.prop6 = t; v.prop7.prop7 = t; v.prop8.prop8 = t; v.prop9.prop9 = t;
        v.prop0.prop0 = t; v.prop1.prop1 = t; v.prop2.prop2 = t; v.prop3.prop3 = t; v.prop4.prop4 = t; v.prop5.prop5 = t; v.prop6.prop6 = t; v.prop7.prop7 = t; v.prop8.prop8 = t; v.prop9.prop9 = t;
        v.prop0.prop0 = t; v.prop1.prop1 = t; v.prop2.prop2 = t; v.prop3.prop3 = t; v.prop4.prop4 = t; v.prop5.prop5 = t; v.prop6.prop6 = t; v.prop7.prop7 = t; v.prop8.prop8 = t; v.prop9.prop9 = t;
        v.prop0.prop0 = t; v.prop1.prop1 = t; v.prop2.prop2 = t; v.prop3.prop3 = t; v.prop4.prop4 = t; v.prop5.prop5 = t; v.prop6.prop6 = t; v.prop7.prop7 = t; v.prop8.prop8 = t; v.prop9.prop9 = t;
        v.prop0.prop0 = t; v.prop1.prop1 = t; v.prop2.prop2 = t; v.prop3.prop3 = t; v.prop4.prop4 = t; v.prop5.prop5 = t; v.prop6.prop6 = t; v.prop7.prop7 = t; v.prop8.prop8 = t; v.prop9.prop9 = t;
        v.prop0.prop0 = t; v.prop1.prop1 = t; v.prop2.prop2 = t; v.prop3.prop3 = t; v.prop4.prop4 = t; v.prop5.prop5 = t; v.prop6.prop6 = t; v.prop7.prop7 = t; v.prop8.prop8 = t; v.prop9.prop9 = t;
        v.prop0.prop0 = t; v.prop1.prop1 = t; v.prop2.prop2 = t; v.prop3.prop3 = t; v.prop4.prop4 = t; v.prop5.prop5 = t; v.prop6.prop6 = t; v.prop7.prop7 = t; v.prop8.prop8 = t; v.prop9.prop9 = t;
        v.prop0.prop0 = t; v.prop1.prop1 = t; v.prop2.prop2 = t; v.prop3.prop3 = t; v.prop4.prop4 = t; v.prop5.prop5 = t; v.prop6.prop6 = t; v.prop7.prop7 = t; v.prop8.prop8 = t; v.prop9.prop9 = t;
        v.prop0.prop0 = t; v.prop1.prop1 = t; v.prop2.prop2 = t; v.prop3.prop3 = t; v.prop4.prop4 = t; v.prop5.prop5 = t; v.prop6.prop6 = t; v.prop7.prop7 = t; v.prop8.prop8 = t; v.prop9.prop9 = t;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("deepSet_2 : " + elapsedTime);
    total += elapsedTime;
}



setProp_1(gP);
setProp_1a(gP);
setProp_1b(gP0, gP1, gP2, gP3, gP4, gP5, gP6, gP7, gP8, gP9);
setProp_2();
setProp_2a();
setProp_2b();
var tP = { };
tP.doit = setProp_3;
tP.doit2 = setProp_3a;
tP.prop1 = { };
tP.doit();
tP.doit2();

deepSet_1();
deepSet_2();

// set the same property into the same object in script scope, at script scope
    var t = { };
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
        gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t; gP.prop1 = t;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("setProp_S : " + elapsedTime);
    total += elapsedTime;

// set different properties into the same object in script scope, at script scope
    var t0 = { };
    var t1 = { };
    var t2 = { };
    var t3 = { };
    var t4 = { };
    var t5 = { };
    var t6 = { };
    var t7 = { };
    var t8 = { };
    var t9 = { };
    startTime = new Date();

    for (i = 0; i < 5000; i++) {
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
        gP.prop0 = t0; gP.prop1 = t1; gP.prop2 = t2; gP.prop3 = t3; gP.prop4 = t4; gP.prop5 = t5; gP.prop6 = t6; gP.prop7 = t7; gP.prop8 = t8; gP.prop9 = t9;
    }
    elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("setProp_Sa : " + elapsedTime);
    total += elapsedTime;


// set different properties into different objects in script scope, at script scope
    t0 = { };
    t1 = { };
    t2 = { };
    t3 = { };
    t4 = { };
    t5 = { };
    t6 = { };
    t7 = { };
    t8 = { };
    t9 = { };
    startTime = new Date();

    for (i = 0; i < 5000; i++) {
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
        gP0.prop0 = t0; gP1.prop1 = t1; gP2.prop2 = t2; gP3.prop3 = t3; gP4.prop4 = t4; gP5.prop5 = t5; gP6.prop6 = t6; gP7.prop7 = t7; gP8.prop8 = t8; gP9.prop9 = t9;
    }
    elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("setProp_Sb : " + elapsedTime);
    total += elapsedTime;
