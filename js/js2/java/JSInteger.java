class JSInteger extends JSNumber {
 
    JSInteger(String s)
    {
        i = new Integer(s).intValue();
    }
    
    JSInteger(int p)
    {
        i = p;
    }
    
    void eval(Environment theEnv)
    {
        theEnv.theStack.push(this);
    }

    JSBoolean toJSBoolean(Environment theEnv) {
        return (i != 0) ? JSBoolean.JSTrue : JSBoolean.JSFalse;
    }
    
    JSDouble toJSDouble(Environment theEnv) {
        return new JSDouble(i);
    }
    
    JSInteger toJSInteger(Environment theEnv) {
        return this;
    }
    
    JSValue toPrimitive(Environment theEnv, String hint) {
        return this;
    }
    
    JSString toJSString(Environment theEnv) {
        return new JSString(Integer.toString(i));
    }
    
    void twiddle(Environment theEnv) {
        theEnv.theStack.push(new JSInteger(~i));
    }

    void and(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push(new JSInteger(i & vR.toJSInteger(theEnv).i));
    }
    
    void or(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push(new JSInteger(i | vR.toJSInteger(theEnv).i));
    }

    void xor(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push(new JSInteger(i ^ vR.toJSInteger(theEnv).i));
    }

    void shl(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push(new JSInteger(i >> vR.toJSInteger(theEnv).i));
    }

    void shr(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push(new JSInteger(i << vR.toJSInteger(theEnv).i));
    }

    void ushl(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push(new JSInteger(i >>> vR.toJSInteger(theEnv).i));
    }

    int i;
    
}