var total="";
print("branch_bench");


// branch on double compare, branch taken (includes cost of var increment)
function branch_VDG()
{
    var t1 = 0.0;

    var a = 1.0;
    var b = 0.0;

    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
    }

    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("branch_VDG : " + elapsedTime);
    total += elapsedTime;
}

// branch on double compare, branch not taken
function branch_VDL()
{
    var t1 = 0.0;

    var a = 0.0;
    var b = 1.0;

    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
    }

    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("branch_VDL : " + elapsedTime);
    total += elapsedTime;
}

// branch on string compare, branch taken
function branch_VSG()
{
    var t1 = 0.0;

    var a = "abcdfghi";
    var b = "abcdefgh";

    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
    }

    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("branch_VSD : " + elapsedTime);
    total += elapsedTime;
}

// branch on string compare, branch not taken
function branch_VSL()
{
    var t1 = 0.0;

    var a = "abcdefgh";
    var b = "abcdfghi";

    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
    }

    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("branch_VSL : " + elapsedTime);
    total += elapsedTime;
}


// branch on parameter compare
function branch_P(a, b, s)
{
    var t1 = 0.0;

    var startTime = new Date();

    for (var i = 0; i < 5000; i++) {
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
        if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++; if (a > b) t1++;
    }

    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("branch_P" + s + " : " + elapsedTime);
    total += elapsedTime;
}

// an empty for loop, iteration count specified by a constant
function for_K()
{
    var t1 = 0.0;

    var startTime = new Date();

    for (var i = 0; i < 1000000; i++) {
    }

    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("for_K : " + elapsedTime);
    total += elapsedTime;
}

// an empty for loop, iteration count specified by a var
function for_V(p)
{
    var t1 = 0.0;
    var c = p;

    var startTime = new Date();

    for (var i = 0; i < c; i++) {
    }

    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("for_V : " + elapsedTime);
    total += elapsedTime;
}

// an empty for loop, iteration count specified by a parameter
function for_P(p)
{
    var t1 = 0.0;

    var startTime = new Date();

    for (var i = 0; i < p; i++) {
    }

    var elapsedTime = (new Date()).getTime() - startTime.getTime();
    print("for_P : " + elapsedTime);
    total += elapsedTime;
}

branch_VDG();
branch_VDL();
branch_VSG();
branch_VSL();

var gA = 1.0;
var gB = 0.0;
branch_P(gA, gB, "DG");

gA = 0.0;
gB = 1.0;
branch_P(gA, gB, "DL");

gA = "abcdfghi";
gB = "abcdefgh";
branch_P(gB, gA, "SG");

gA = "abcdefgh";
gB = "abcdfghi";
branch_P(gB, gA, "SL");


for_K();
for_V(1000000);
for_P(1000000);
