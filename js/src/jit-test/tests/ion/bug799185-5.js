function foo(aObject)
{
    try {
        try {
            if (!aObject)
                return;
        }
        catch (ex if (ex.name == "TypeError")) { }
        finally {
        }
        undefined.x;
    }
    catch (ex if (ex.name == "TypeError")) { }
    catch (ex if (ex.name == "TypeError")) { }
    finally {
    }
}

foo(true);
