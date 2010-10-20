/* vim: set ts=4 sw=4 tw=99 et: */
function foundit(items, n) {
    for (var i = 0; i < 10; i++)
        arguments[2](items, this);
}

function dostuff() {
    print(this);
}
foundit('crab', 'crab', dostuff);

/* Don't crash or assert */

