var objs;

function init()
{
    objs = new Object();
    var x = new Object();
    objs.root = x;
    x.a = new Object();
    x.b = new Object();

    /*
     * Clears out the arena lists. Otherwise all the objects above
     * would be considered to be created during the incremental GC.
     */
    gc();
}

/*
 * Use eval here so that the interpreter frames end up higher on the
 * stack, which avoids them being seen later on by the conservative
 * scanner.
 */
eval("init()");

gcslice(0); // Start IGC, but don't mark anything.
selectforgc(objs.root);
gcslice(1);
delete objs.root.b;
delete objs.root.a;
gcslice();
