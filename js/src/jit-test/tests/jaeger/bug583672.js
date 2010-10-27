(function () {
    function f()
    {
        this.y = w
        this.y = (void 0)
        Object
    }
    for (a in [0, 0, 0, 0])
    {
        new f
    }
    let w = {}
})()

/* Make sure that MICs don't have the same bug. */
x = Object();
(function () {
    function f()
    {
        x = w
        x = (void 0)
        Object
    }
    for (a in [0, 0, 0, 0])
    {
        new f
    }
    let w = {}
})()
/* Don't assert. */
