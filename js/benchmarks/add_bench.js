
var total="";
print("add_bench");

// add two double parameters
function addDouble_P(d1, d2)
{
    var t;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("addDouble_P : " + elapsedTime);
    total += elapsedTime;
}

// add a double parameter and a double literal
function addDouble_PK(d1)
{
    var t;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("addDouble_PK : " + elapsedTime);
    total += elapsedTime;
}

// add two double local vars
function addDouble_V()
{
    var t;
    var d1 = 2.781828;
    var d2 = 3.1415926535;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
        t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2; t = d1 + d2;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("addDouble_V : " + elapsedTime);
    total += elapsedTime;
}

// add a double var and a double literal
function addDouble_VK(d1)
{
    var t;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
        t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136; t = d1 + 1.4142136;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("addDouble_VK : " + elapsedTime);
    total += elapsedTime;
}

// add two string parameters (each 8 bytes long)
function addString_P8(s1, s2)
{
    var t;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("addString_P8 : " + elapsedTime);
    total += elapsedTime;
}

// add two string parameters (each 64 bytes long)
function addString_P64(s1, s2)
{
    var t;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("addString_P64 : " + elapsedTime);
    total += elapsedTime;
}

// add two string local vars (each 8 bytes long)
function addString_V8()
{
    var t;
    var s1 = "01234567";
    var s2 = "abcdefgh";
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("addString_V8 : " + elapsedTime);
    total += elapsedTime;
}

// add two string local vars (each 8 bytes long)
function addString_V64()
{
    var t;
    var s1 = "01234567";
    var s2 = "abcdefgh";
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
        t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2; t = s1 + s2;
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("addString_V64 : " + elapsedTime);
    total += elapsedTime;
}


var gD1 = 2.781828;
var gD2 = 3.1415926535;
var gS1_8 = "12345678";
var gS2_8 = "ABCDEFGH"

var gS1_64 = "12345678abcdefgh12345678abcdefgh12345678abcdefgh12345678abcdefgh";
var gS2_64 = "ZYXWVUTS........ZYXWVUTS........ZYXWVUTS........ZYXWVUTS........"

addDouble_P(gD1, gD2);
addDouble_PK(gD1);
addDouble_V();
addDouble_VK(gD1);

addString_P8(gS1_8, gS2_8);
addString_P64(gS1_64, gS2_64);

addString_V8();
addString_V64();

