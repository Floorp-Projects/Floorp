function foo(aObject)
{
    try { }
    catch (ex) {
        if (ex.name != "TypeError")
            throw ex;
    }
    try { Object.getPrototypeOf(aObject); }
    catch (ex) { }
}

foo(true);
