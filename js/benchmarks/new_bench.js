var total="";
print("new_bench");

// construct (and release) lots of objects
function new_1()
{
    var t;
    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { };
        t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { };
        t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { };
        t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { };
        t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { };
        t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { };
        t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { };
        t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { };
        t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { };
        t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { };
        t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { };
        t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { };
        t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { };
        t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { };
        t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { };
        t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { };
        t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { };
        t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { };
        t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { };
        t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { }; t = { };
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("new_1 : " + elapsedTime);
    total += elapsedTime;
}


new_1();
