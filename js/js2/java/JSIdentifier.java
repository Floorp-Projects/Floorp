class JSIdentifier extends JSString {

    JSIdentifier(String s)
    {
        super(s);
    }
    
    String print(String indent)
    {
        return indent + "JSIdentifier : " + s + "\n";
    }

    JSValue eval(Environment theEnv)
    {
        return theEnv.scope.getProp(theEnv, this);
    }
    
    JSReference evalLHS(Environment theEnv)
    {
        return new JSReference(theEnv.scope, this);
    }
    
}