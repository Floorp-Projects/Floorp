var total="";
print("array_bench");


// access the same array element, from a locally constructed, finite size array - with double index
function array_1()
{
    var t = { };

    var a = new Array(100);
    a[33] = { };


    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("array_1 : " + elapsedTime);
    total += elapsedTime;
}

// access the same array element, from a locally constructed, un-finite size array - with double index
function array_2()
{
    var t = { };

    var a = { };
    a[33] = { };

    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
        t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33]; t = a[33];
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("array_2 : " + elapsedTime);
    total += elapsedTime;
}


// access the same array element, from a locally constructed, finite size array - with string index
function array_3()
{
    var t = { };

    var a = new Array(100);
    a["33"] = { };

    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"];
        t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"];
        t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"];
        t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"];
        t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"];
        t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"];
        t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"];
        t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"];
        t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"];
        t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"];
        t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"];
        t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"];
        t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"];
        t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"];
        t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"];
        t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"];
        t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"];
        t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"];
        t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"];
        t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"]; t = a["33"];
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("array_3 : " + elapsedTime);
    total += elapsedTime;
}

// access the same array element, from a locally constructed, finite size array - with var index
function array_4()
{
    var t = { };

    var a = new Array(100);
    var x = 33;
    a[x] = { };

    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x];
        t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x];
        t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x];
        t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x];
        t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x];
        t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x];
        t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x];
        t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x];
        t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x];
        t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x];
        t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x];
        t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x];
        t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x];
        t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x];
        t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x];
        t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x];
        t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x];
        t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x];
        t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x];
        t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x]; t = a[x];
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("array_3 : " + elapsedTime);
    total += elapsedTime;
}

// store into incrementing indices in a finite sized array
function array_5()
{
    var t = { };

    var a = new Array(100);

    var startTime = new Date();

    for (var i = 0; i < 1000; i++) {
        for (var j = 0; j < 100; j++) a[j] = t;
    }

    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("array_5 : " + elapsedTime);
    total += elapsedTime;
}

// store into incrementing indices in an un-finite sized array
function array_6()
{
    var t = { };

    var a = { };

    var startTime = new Date();

    for (var i = 0; i < 1000; i++) {
        for (var j = 0; j < 100; j++) a[j] = t;
    }

    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("array_6 : " + elapsedTime);
    total += elapsedTime;
}

array_1();
array_2();
array_3();
array_4();
array_5();
array_6();
