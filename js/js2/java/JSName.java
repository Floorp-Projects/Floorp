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
        JSScope scope = theEnv.scope;
        while (scope != null) {
            if (scope.hasProp(theEnv, id))
                return new JSReference(scope, id);
            else
                scope = scope.parent;
        }
        return new JSReference(theEnv.globalScope, id);
    }
    
    JSValue eval(Environment theEnv)
    {
        JSScope scope = theEnv.scope;
        while (scope != null) {
            if (scope.hasProp(theEnv, id))
                return scope.getProp(theEnv, id);
            else
                scope = scope.parent;
        }
        throw new JSException(new JSString(id.s + " undefined"));
    }
    
    JSIdentifier id;
    int scope;
    
}