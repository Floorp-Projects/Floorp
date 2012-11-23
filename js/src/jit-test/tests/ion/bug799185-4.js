function foo(aObject)
{
    try { }
    catch (ex if (ex.name == "TypeError")) { }
    try { Object.getPrototypeOf(aObject); }
    catch (ex) { }
}

foo(true);
