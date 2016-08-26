// |jit-test| error: ReferenceError

{
    for (var x = 0; i < 100; i++) a >>= i
    let i = 1;
}
