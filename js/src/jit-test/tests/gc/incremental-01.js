var objs;

function init()
{
    objs = new Object();
    var x = new Object();
    objs.root1 = x;
    objs.root2 = new Object();
    x.ptr = new Object();
    x = null;

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
selectforgc(objs.root2);
gcslice(1);
objs.root2.ptr = objs.root1.ptr;
objs.root1.ptr = null;
gcslice();
