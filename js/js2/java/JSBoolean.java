class JSBoolean extends JSValue {
    
    static JSBoolean JSTrue = new JSBoolean(true);
    static JSBoolean JSFalse = new JSBoolean(false);

    private JSBoolean(boolean p)
    {
        b = p;
    }
    
    void eval(Environment theEnv)
    {
        theEnv.theStack.push(this);
    }
    
    boolean isTrue()
    {
        return b;
    }
            
    boolean isFalse()
    {
        return !b;
    }
    
    JSBoolean toJSBoolean() {
        return this;
    }
    
            
    boolean b;
    
}