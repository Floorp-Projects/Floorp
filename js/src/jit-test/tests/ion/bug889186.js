function f()
{
        return (4 >>> 0) > ((0 % (1 == 2)) >>> 0);
}
assertEq(f(), true);
assertEq(f(), true);
