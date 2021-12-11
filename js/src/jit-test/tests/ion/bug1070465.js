// |jit-test| error: ReferenceError
{
  while (x && 0) {}
  let x
}
