class JSString extends JSValue {
 
    JSString(String p)
    {
        s = p;
    }
    
    void eval(Environment theEnv)
    {
        theEnv.theStack.push(this);
    }
    
    void typeof(Environment theEnv) {
        theEnv.theStack.push(new JSString("string"));
    }

    void add(Environment theEnv)
    {
        JSString vR = theEnv.theStack.pop().toJSString(theEnv);
        theEnv.theStack.push(new JSString(s + vR.s));
    }

    void gt(Environment theEnv) {
        JSValue vR = theEnv.theStack.peek();
        if (vR instanceof JSString) {
            theEnv.theStack.pop();
            theEnv.theStack.push((s.compareTo(vR.toJSString(theEnv).s) == 1) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
        }
        else
            toJSDouble(theEnv).gt(theEnv);
    }

    void ge(Environment theEnv) {
        JSValue vR = theEnv.theStack.peek();
        if (vR instanceof JSString) {
            theEnv.theStack.pop();
            theEnv.theStack.push((s.compareTo(vR.toJSString(theEnv).s) != -1) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
        }
        else
            toJSDouble(theEnv).ge(theEnv);
    }
    
    void lt(Environment theEnv) {
        JSValue vR = theEnv.theStack.peek();
        if (vR instanceof JSString) {
            theEnv.theStack.pop();
            theEnv.theStack.push((s.compareTo(vR.toJSString(theEnv).s) == -1) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
        }
        else
            toJSDouble(theEnv).lt(theEnv);
    }
    
    void le(Environment theEnv) {
        JSValue vR = theEnv.theStack.peek();
        if (vR instanceof JSString) {
            theEnv.theStack.pop();
            theEnv.theStack.push((s.compareTo(vR.toJSString(theEnv).s) != 1) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
        }
        else
            toJSDouble(theEnv).le(theEnv);
    }
    
    void eq(Environment theEnv) {
        JSValue vR = theEnv.theStack.peek();
        if (vR instanceof JSString) {
            theEnv.theStack.pop();
            theEnv.theStack.push((s.compareTo(vR.toJSString(theEnv).s) == 0) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
        }
        else
            toJSDouble(theEnv).eq(theEnv);
    }
    
    void ne(Environment theEnv) {
        JSValue vR = theEnv.theStack.peek();
        if (vR instanceof JSString) {
            theEnv.theStack.pop();
            theEnv.theStack.push((s.compareTo(vR.toJSString(theEnv).s) != 0) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
        }
        else
            toJSDouble(theEnv).ne(theEnv);
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