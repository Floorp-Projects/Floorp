// |jit-test| error: ReferenceError

{
    while (x && 0)
        if (!((x = 1) === x)) {}
    let x = () => sym()
}
