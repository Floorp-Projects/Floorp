class JSString extends JSValue {
 
    JSString(String p)
    {
        s = p;
    }
    
    JSValue eval(Environment theEnv)
    {
        return this;
    }
    
    JSValue typeof(Environment theEnv) {
        return new JSString("string");
    }

    JSValue add(Environment theEnv, JSValue rV)
    {
        return new JSString(s + rV.toJSString(theEnv).s);
    }

    JSValue gt(Environment theEnv, JSValue rV) {
        if (rV instanceof JSString)
            return (s.compareTo(rV.toJSString(theEnv).s) == 1) ? JSBoolean.JSTrue : JSBoolean.JSFalse;
        else
            return toJSDouble(theEnv).gt(theEnv, rV);
    }

    JSValue ge(Environment theEnv, JSValue rV) {
        if (rV instanceof JSString)
            return (s.compareTo(rV.toJSString(theEnv).s) != -1) ? JSBoolean.JSTrue : JSBoolean.JSFalse;
        else
            return toJSDouble(theEnv).ge(theEnv, rV);
    }
    
    JSValue lt(Environment theEnv, JSValue rV) {
        if (rV instanceof JSString)
            return (s.compareTo(rV.toJSString(theEnv).s) == -1) ? JSBoolean.JSTrue : JSBoolean.JSFalse;
        else
            return toJSDouble(theEnv).lt(theEnv, rV);
    }
    
    JSValue le(Environment theEnv, JSValue rV) {
        if (rV instanceof JSString)
            return (s.compareTo(rV.toJSString(theEnv).s) != 1) ? JSBoolean.JSTrue : JSBoolean.JSFalse;
        else
            return toJSDouble(theEnv).le(theEnv, rV);
    }
    
    JSValue eq(Environment theEnv, JSValue rV) {
        if (rV instanceof JSString)
            return (s.compareTo(rV.toJSString(theEnv).s) == 0) ? JSBoolean.JSTrue : JSBoolean.JSFalse;
        else
            return toJSDouble(theEnv).eq(theEnv, rV);
    }
    
    JSValue ne(Environment theEnv, JSValue rV) {
        if (rV instanceof JSString)
            return (s.compareTo(rV.toJSString(theEnv).s) != 0) ? JSBoolean.JSTrue : JSBoolean.JSFalse;
        else
            return toJSDouble(theEnv).ne(theEnv, rV);
    }
    
    JSDouble toJSDouble(Environment theEnv) {
        return new JSDouble(s);         // XXX Way More To Do, see Rhino ScriptRuntime.java
    }
    
    JSString toJSString(Environment theEnv) {
        return this;
    }
    
    JSValue toPrimitive(Environment theEnv, String hint) {
        return this;
    }
    
    String print(String indent)
    {
        return indent + "JSString : " + s + "\n";
    }

    protected String s;
    
}