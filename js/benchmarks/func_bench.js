var total="";
print("func_bench");

// call a function with zero parameters, returning nothing
function f_0V()
{
}

function call_0V()
{
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V();
        f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V();
        f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V();
        f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V();
        f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V();
        f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V();
        f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V();
        f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V();
        f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V();
        f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V();
        f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V();
        f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V();
        f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V();
        f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V();
        f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V();
        f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V();
        f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V();
        f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V();
        f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V();
        f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V(); f_0V();
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("call_0V : " + elapsedTime);
    total += elapsedTime;
}


var gB = { };

// call a function with zero parameters, returning an object
function f_0B()
{
    return gB;
}

function call_0B()
{
    var t;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B();
        t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B();
        t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B();
        t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B();
        t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B();
        t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B();
        t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B();
        t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B();
        t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B();
        t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B();
        t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B();
        t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B();
        t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B();
        t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B();
        t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B();
        t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B();
        t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B();
        t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B();
        t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B();
        t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B(); t = f_0B();
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("call_0B : " + elapsedTime);
    total += elapsedTime;
}

// call a function with one parameter, returning an object
function f_1B(a)
{
    return gB;
}

function call_1B()
{
    var t;
    var p = { };
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p);
        t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p);
        t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p);
        t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p);
        t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p);
        t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p);
        t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p);
        t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p);
        t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p);
        t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p);
        t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p);
        t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p);
        t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p);
        t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p);
        t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p);
        t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p);
        t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p);
        t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p);
        t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p);
        t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p); t = f_1B(p);
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("call_1B : " + elapsedTime);
    total += elapsedTime;
}

// call a function with two parameters, returning an object
function f_2B(a, b)
{
    return gB;
}

function call_2B()
{
    var t;
    var p0 = { };
    var p1 = { };
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1);
        t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1);
        t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1);
        t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1);
        t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1);
        t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1);
        t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1);
        t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1);
        t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1);
        t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1);
        t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1);
        t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1);
        t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1);
        t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1);
        t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1);
        t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1);
        t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1);
        t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1);
        t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1);
        t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1); t = f_2B(p0, p1);
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("call_2B : " + elapsedTime);
    total += elapsedTime;
}


// call a function with four parameters, returning an object
function f_4B(a, b, c, d)
{
    return gB;
}

function call_4B()
{
    var t;
    var p0 = { };
    var p1 = { };
    var p2 = { };
    var p3 = { };
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3);
        t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3);
        t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3);
        t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3);
        t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3);
        t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3);
        t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3);
        t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3);
        t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3);
        t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3);
        t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3);
        t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3);
        t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3);
        t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3);
        t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3);
        t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3);
        t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3);
        t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3);
        t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3);
        t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3); t = f_4B(p0, p1, p2, p3);
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("call_4B : " + elapsedTime);
    total += elapsedTime;
}

// call a function with eight parameters, returning an object
function f_8B(a, b, c, d, e, f, g, h)
{
    return gB;
}

function call_8B()
{
    var t;
    var p0 = { };
    var p1 = { };
    var p2 = { };
    var p3 = { };
    var p4 = { };
    var p5 = { };
    var p6 = { };
    var p7 = { };
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7);
        t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7);
        t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7);
        t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7);
        t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7);
        t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7);
        t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7);
        t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7);
        t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7);
        t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7);
        t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7);
        t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7);
        t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7);
        t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7);
        t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7);
        t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7);
        t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7);
        t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7);
        t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7);
        t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7); t = f_8B(p0, p1, p2, p3, p4, p5,p6, p7);
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("call_8B : " + elapsedTime);
    total += elapsedTime;
}

// call a function passing through four parameters
function f_4P(a, b, c, d)
{
    return gB;
}

function call_4P(p0, p1, p2, p3)
{
    var t;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3);
        t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3);
        t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3);
        t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3);
        t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3);
        t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3);
        t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3);
        t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3);
        t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3);
        t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3);
        t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3);
        t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3);
        t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3);
        t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3);
        t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3);
        t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3);
        t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3);
        t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3);
        t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3);
        t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3); t = f_4P(p0, p1, p2, p3);
    }

    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("call_4P : " + elapsedTime);
    total += elapsedTime;
}

var gP0 = { };
var gP1 = { };
var gP2 = { };
var gP3 = { };

call_0V();
call_0B();
call_1B();
call_2B();
call_4B();
call_8B();
call_4P(gP0, gP1, gP2, gP3);
