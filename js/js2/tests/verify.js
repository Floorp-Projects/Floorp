count = 0;

function verify(a, b)
{
    if ((a == b) || ((b != b) && (a == undefined)) )
        print("Test " + count + " succeeded");
    else
        print("Test " + count + " failed, expected " + b + ", got " + a);
    count++;
}

