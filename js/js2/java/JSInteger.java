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

    JSBoolean toJSBoolean() {
        return (i != 0) ? JSBoolean.JSTrue : JSBoolean.JSFalse;
    }
    
    JSDouble toJSDouble() {
        return new JSDouble(i);
    }
    
    JSInteger toJSInteger() {
        return this;
    }
    
    JSValue toPrimitive() {
        return this;
    }
    
    JSString toJSString() {
        return new JSString(Integer.toString(i));
    }
    
    void twiddle(Environment theEnv) {
        theEnv.theStack.push(new JSInteger(~i));
    }

    void and(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push(new JSInteger(i & vR.toJSInteger().i));
    }
    
    void or(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push(new JSInteger(i | vR.toJSInteger().i));
    }

    void xor(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push(new JSInteger(i ^ vR.toJSInteger().i));
    }

    void shl(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push(new JSInteger(i >> vR.toJSInteger().i));
    }

    void shr(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push(new JSInteger(i << vR.toJSInteger().i));
    }

    void ushl(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push(new JSInteger(i >>> vR.toJSInteger().i));
    }

    int i;
    
}