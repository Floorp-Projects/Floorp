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
    
    JSValue toPrimitive(String hint) {
        return this;
    }
    
    JSString toJSString() {
        return (b) ? new JSString("true") : new JSString("false");
    }
    
    JSDouble toJSDouble() {
        return (b) ? new JSDouble(1) : new JSDouble(0);
    }
            
    boolean b;
    
}