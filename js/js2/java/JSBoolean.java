class JSBoolean extends JSValue {
    
    static JSBoolean JSTrue = new JSBoolean(true);
    static JSBoolean JSFalse = new JSBoolean(false);

    private JSBoolean(boolean p)
    {
        b = p;
    }
    
    JSValue eval(Environment theEnv)
    {
        return this;
    }
    
    boolean isTrue()
    {
        return b;
    }
            
    boolean isFalse()
    {
        return !b;
    }
    
    JSValue bang(Environment theEnv) {
         return (b) ? JSFalse : JSTrue;
    }

    JSValue typeof(Environment theEnv) {
        return new JSString("boolean");
    }

    JSBoolean toJSBoolean(Environment theEnv) {
        return this;
    }
    
    JSValue toPrimitive(Environment theEnv, String hint) {
        return this;
    }
    
    JSString toJSString(Environment theEnv) {
        return (b) ? new JSString("true") : new JSString("false");
    }
    
    JSDouble toJSDouble(Environment theEnv) {
        return (b) ? new JSDouble(1) : new JSDouble(0);
    }
            
    boolean b;
    
}