function f()
{
  arguments['4294967295'] = 2;
}
assertEq(f(), undefined);
