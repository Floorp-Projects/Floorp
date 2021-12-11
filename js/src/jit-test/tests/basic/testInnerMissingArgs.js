function innerTestInnerMissingArgs(a,b,c,d)
{
        if (a) {
        } else {
        }
}

function doTestInnerMissingArgs(k)
{
    for (i = 0; i < 10; i++) {
        innerTestInnerMissingArgs(k);
    }
}

function testInnerMissingArgs()
{
    doTestInnerMissingArgs(1);
    doTestInnerMissingArgs(0);
}

testInnerMissingArgs();
