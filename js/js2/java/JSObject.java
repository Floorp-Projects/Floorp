class JSObject extends JSValue {
    
    static JSObject JSUndefined = new JSObject("undefined");
    
    JSObject(String aValue)
    {
        value = aValue;
    }
    
    String print(String indent)
    {
        return indent + "JSObject : " + value + "\n";
    }

    void evalLHS(Environment theEnv)
    {
        theEnv.theStack.push(this);
    }
    
    void eval(Environment theEnv)
    {
        JSValue v = (JSValue)(theEnv.theGlobals.get(value));
        if (v == null) {
            System.out.println("Accessed undefined : " + value);
            theEnv.theStack.push(JSUndefined);
        }
        else
            theEnv.theStack.push(v);
    }    
        
    public String toString()
    {
        return value;
    }
        
    String value;
    
}