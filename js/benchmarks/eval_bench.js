var total="";
print("eval_bench.js");

// assign to a var from within an eval
function eval_1()
{
    var t = { };
    var q = { };

    var startTime = new Date();

    for (var i = 0; i < 5; i++) {
        eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;");
        eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;");
        eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;");
        eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;");
        eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;");
        eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;");
        eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;");
        eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;");
        eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;");
        eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;");
        eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;");
        eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;");
        eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;");
        eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;");
        eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;");
        eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;");
        eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;");
        eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;");
        eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;");
        eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;"); eval("t = q;");
    }
    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("eval_1 : " + elapsedTime);
    total += elapsedTime;
}


eval_1();
