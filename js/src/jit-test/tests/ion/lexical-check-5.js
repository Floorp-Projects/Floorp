// |jit-test| error: ReferenceError

{
  for (var i = 0; i < 100; i++)
    a += i;
  let a = 1;
}
