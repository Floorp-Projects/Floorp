/* Don't assert. */

function f(o) {
  o == 1;
    if (o == 2) {}
}
for (var i = 0; i < 20; i++)
  f(3.14);
