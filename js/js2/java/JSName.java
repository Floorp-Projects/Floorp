class JSName extends ExpressionNode {

    JSName(JSIdentifier anID, int aScope)
    {
        id = anID;
        scope = aScope;         // this is the scope that the name was used in
    }
    
    String print(String indent)
    {
        return indent + "JSName : " + id.s + ", scope : " + scope + "\n";
    }

    JSReference evalLHS(Environment theEnv)
    {
        return new JSReference(theEnv.scope, id);
    }
    
    JSValue eval(Environment theEnv)
    {
        return theEnv.scope.getProp(theEnv, id);
    }
    
    JSIdentifier id;
    int scope;
    
}