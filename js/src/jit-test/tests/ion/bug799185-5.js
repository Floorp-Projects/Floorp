function foo(aObject)
{
    try {
        try {
            if (!aObject)
                return;
        }
        catch (ex) {
            if (ex.name != "TypeError")
                throw ex;
        }
        finally {
        }
        undefined.x;
    }
    catch (ex) {
        if (ex.name != "TypeError")
            throw ex;
        if (ex.name != "TypeError")
            throw ex;
    }
    finally {
    }
}

foo(true);
