function f(y) {
    if (y)
        return;
    {
        let x;
        for (;;) {}
    }
}


/* Don't assert. */
f(1);

