var total="";
print("inc_bench");

// increment a parameter
function increment_P(p)
{
    var t;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        p++; p++; p++; p++; p++; p++; p++; p++; p++; p++;
        p++; p++; p++; p++; p++; p++; p++; p++; p++; p++;
        p++; p++; p++; p++; p++; p++; p++; p++; p++; p++;
        p++; p++; p++; p++; p++; p++; p++; p++; p++; p++;
        p++; p++; p++; p++; p++; p++; p++; p++; p++; p++;
        p++; p++; p++; p++; p++; p++; p++; p++; p++; p++;
        p++; p++; p++; p++; p++; p++; p++; p++; p++; p++;
        p++; p++; p++; p++; p++; p++; p++; p++; p++; p++;
        p++; p++; p++; p++; p++; p++; p++; p++; p++; p++;
        p++; p++; p++; p++; p++; p++; p++; p++; p++; p++;
        p++; p++; p++; p++; p++; p++; p++; p++; p++; p++;
        p++; p++; p++; p++; p++; p++; p++; p++; p++; p++;
        p++; p++; p++; p++; p++; p++; p++; p++; p++; p++;
        p++; p++; p++; p++; p++; p++; p++; p++; p++; p++;
        p++; p++; p++; p++; p++; p++; p++; p++; p++; p++;
        p++; p++; p++; p++; p++; p++; p++; p++; p++; p++;
        p++; p++; p++; p++; p++; p++; p++; p++; p++; p++;
        p++; p++; p++; p++; p++; p++; p++; p++; p++; p++;
        p++; p++; p++; p++; p++; p++; p++; p++; p++; p++;
        p++; p++; p++; p++; p++; p++; p++; p++; p++; p++;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("increment_P : " + elapsedTime);
    total += elapsedTime;
}

// increment a var
function increment_V(p)
{
    var t;
    var v = 0.0;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        v++; v++; v++; v++; v++; v++; v++; v++; v++; v++;
        v++; v++; v++; v++; v++; v++; v++; v++; v++; v++;
        v++; v++; v++; v++; v++; v++; v++; v++; v++; v++;
        v++; v++; v++; v++; v++; v++; v++; v++; v++; v++;
        v++; v++; v++; v++; v++; v++; v++; v++; v++; v++;
        v++; v++; v++; v++; v++; v++; v++; v++; v++; v++;
        v++; v++; v++; v++; v++; v++; v++; v++; v++; v++;
        v++; v++; v++; v++; v++; v++; v++; v++; v++; v++;
        v++; v++; v++; v++; v++; v++; v++; v++; v++; v++;
        v++; v++; v++; v++; v++; v++; v++; v++; v++; v++;
        v++; v++; v++; v++; v++; v++; v++; v++; v++; v++;
        v++; v++; v++; v++; v++; v++; v++; v++; v++; v++;
        v++; v++; v++; v++; v++; v++; v++; v++; v++; v++;
        v++; v++; v++; v++; v++; v++; v++; v++; v++; v++;
        v++; v++; v++; v++; v++; v++; v++; v++; v++; v++;
        v++; v++; v++; v++; v++; v++; v++; v++; v++; v++;
        v++; v++; v++; v++; v++; v++; v++; v++; v++; v++;
        v++; v++; v++; v++; v++; v++; v++; v++; v++; v++;
        v++; v++; v++; v++; v++; v++; v++; v++; v++; v++;
        v++; v++; v++; v++; v++; v++; v++; v++; v++; v++;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("increment_V : " + elapsedTime);
    total += elapsedTime;
}


// increment a script scope var
var gV = 0.0;
function increment_gV()
{
    var t;
    var v = 0.0;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++;
        gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++;
        gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++;
        gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++;
        gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++;
        gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++;
        gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++;
        gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++;
        gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++;
        gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++;
        gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++;
        gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++;
        gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++;
        gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++;
        gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++;
        gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++;
        gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++;
        gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++;
        gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++;
        gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++; gV++;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("increment_gV : " + elapsedTime);
    total += elapsedTime;
}

// increment a property of an object passed as a parameter
function increment_PP(p)
{
    var t;
    var v = 0.0;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++;
        p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++;
        p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++;
        p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++;
        p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++;
        p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++;
        p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++;
        p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++;
        p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++;
        p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++;
        p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++;
        p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++;
        p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++;
        p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++;
        p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++;
        p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++;
        p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++;
        p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++;
        p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++;
        p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++; p.v++;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("increment_PP : " + elapsedTime);
    total += elapsedTime;
}

var x = 0.0;
increment_P(x);
increment_V();
increment_gV();
var gP = { };
gP.v = 0.0;
increment_PP(gP);

