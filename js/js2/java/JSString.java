class JSString extends JSValue {
 
    JSString(String p)
    {
        s = p;
    }
    
    void eval(Environment theEnv)
    {
        theEnv.theStack.push(this);
    }
    
    void add(Environment theEnv)
    {
        JSString vR = theEnv.theStack.pop().toJSString();
        theEnv.theStack.push(new JSString(s + vR.s));
    }

    void gt(Environment theEnv) {
        JSValue vR = theEnv.theStack.peek();
        if (vR instanceof JSString) {
            theEnv.theStack.pop();
            theEnv.theStack.push((s.compareTo(vR.toJSString().s) == 1) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
        }
        else
            toJSDouble().gt(theEnv);
    }

    void ge(Environment theEnv) {
        JSValue vR = theEnv.theStack.peek();
        if (vR instanceof JSString) {
            theEnv.theStack.pop();
            theEnv.theStack.push((s.compareTo(vR.toJSString().s) != -1) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
        }
        else
            toJSDouble().ge(theEnv);
    }
    
    void lt(Environment theEnv) {
        JSValue vR = theEnv.theStack.peek();
        if (vR instanceof JSString) {
            theEnv.theStack.pop();
            theEnv.theStack.push((s.compareTo(vR.toJSString().s) == -1) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
        }
        else
            toJSDouble().lt(theEnv);
    }
    
    void le(Environment theEnv) {
        JSValue vR = theEnv.theStack.peek();
        if (vR instanceof JSString) {
            theEnv.theStack.pop();
            theEnv.theStack.push((s.compareTo(vR.toJSString().s) != 1) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
        }
        else
            toJSDouble().le(theEnv);
    }
    
    void eq(Environment theEnv) {
        JSValue vR = theEnv.theStack.peek();
        if (vR instanceof JSString) {
            theEnv.theStack.pop();
            theEnv.theStack.push((s.compareTo(vR.toJSString().s) == 0) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
        }
        else
            toJSDouble().eq(theEnv);
    }
    
    void ne(Environment theEnv) {
        JSValue vR = theEnv.theStack.peek();
        if (vR instanceof JSString) {
            theEnv.theStack.pop();
            theEnv.theStack.push((s.compareTo(vR.toJSString().s) != 0) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
        }
        else
            toJSDouble().ne(theEnv);
    }
    
    JSDouble toJSDouble() {
        return new JSDouble(s);         // XXX Way More To Do, see Rhino ScriptRuntime.java
    }
    
    JSString toJSString() {
        return this;
    }
    
    JSValue toPrimitive() {
        return this;
    }
    
    String s;
    
}