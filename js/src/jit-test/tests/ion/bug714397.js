// Don't assert. Reduced from a modified SS 1.0 crypto-md5.

function g()
{
  return 0;
}

function f()
{
  for(var i = 0; i < 100; i++) {
    g(0);
    g(0);
  }
}


f();
