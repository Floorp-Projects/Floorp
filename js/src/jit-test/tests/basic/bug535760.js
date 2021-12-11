/* vim: set ts=8 sts=4 et sw=4 tw=99: */
function foundit(items, n) {
    for (var i = 0; i < 10; i++)
        arguments[2](items, this);
}

function dostuff() {
    print(this);
}
foundit('crab', 'crab', dostuff);

/* Don't crash or assert */

