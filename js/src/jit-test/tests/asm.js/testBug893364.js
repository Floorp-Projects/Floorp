function m()
{
    "use asm";
    function f()
    {
        var x = 0;
        var y = 0;
        x = (((0x77777777 - 0xcccccccc) | 0) % -1) | 0;
        y = (((0x7FFFFFFF + 0x7FFFFFFF) | 0) % -1) | 0;
        return (x+y)|0;
    }
    return f;
}
assertEq(m()(), 0)
