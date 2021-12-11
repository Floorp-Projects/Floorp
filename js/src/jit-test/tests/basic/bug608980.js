
/* Don't trip bogus assert. */

function foo()
{
  var x;
  while (x = 0) {
    x = 1;
  }
}
foo();
