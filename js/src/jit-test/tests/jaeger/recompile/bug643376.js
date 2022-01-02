SECTION = 0;
function TestCase() {}
function outer_func(x)
{
    var y = "inner";
    new TestCase( SECTION, { SECTION: ++y });
}
outer_func(1111);
