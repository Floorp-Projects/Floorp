class JSIdentifier extends JSString {

    JSIdentifier(String s)
    {
        super(s);
    }
    
    String print(String indent)
    {
        return indent + "JSIdentifier : " + s + "\n";
    }

    void eval(Environment theEnv)
    {
        theEnv.theStack.push(this);
        theEnv.scope.getProp(theEnv);
    }
    
    void evalLHS(Environment theEnv)
    {
        theEnv.theStack.push(this);
        theEnv.theStack.push(theEnv.scope);
    }
    
}