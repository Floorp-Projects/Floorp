class JSDouble extends JSNumber {
 
    JSDouble(double p)
    {
        d = p;
    }
    
    JSDouble(String s)
    {
        d = new Double(s).doubleValue();
    }
    
    String print(String indent)
    {
        return indent + "JSDouble " + d + "\n";
    }

    public String toString()
    {
        return Double.toString(d);
    }
    
    void eval(Environment theEnv)
    {
        theEnv.theStack.push(this);
    }

    void plus(Environment theEnv) {
        theEnv.theStack.push(this);
    }
    
    void minus(Environment theEnv) {
        theEnv.theStack.push(new JSDouble(-d));
    }
    
    void add(Environment theEnv) {
        JSValue vR = theEnv.theStack.peek();
        if (vR instanceof JSString)         
            toJSString(theEnv).add(theEnv);
        else {
            theEnv.theStack.pop();
            theEnv.theStack.push(new JSDouble(d + vR.toJSDouble(theEnv).d));
        }
    }
    
    void subtract(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push(new JSDouble(d - vR.toJSDouble(theEnv).d));
    }
    
    void multiply(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push(new JSDouble(d * vR.toJSDouble(theEnv).d));
    }

    void divide(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push(new JSDouble(d / vR.toJSDouble(theEnv).d));
    }
    
    void remainder(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push(new JSDouble(d % vR.toJSDouble(theEnv).d));
    }
    
    void gt(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push((d > vR.toJSDouble(theEnv).d) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
    }

    void ge(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push((d >= vR.toJSDouble(theEnv).d) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
    }
    
    void lt(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push((d < vR.toJSDouble(theEnv).d) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
    }
    
    void le(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push((d <= vR.toJSDouble(theEnv).d) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
    }
    
    void eq(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push((d == vR.toJSDouble(theEnv).d) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
    }
    
    void ne(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push((d != vR.toJSDouble(theEnv).d) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
    }
    
    JSDouble toJSDouble(Environment theEnv) {
        return this;
    }
    
    JSValue toPrimitive(Environment theEnv, String hint) {
        return this;
    }
    
    JSInteger toJSInteger(Environment theEnv) {
        return new JSInteger((int)d);
    }
    
    JSBoolean toJSBoolean(Environment theEnv) {
        return ((d == d) && (d != 0.0)) ? JSBoolean.JSTrue : JSBoolean.JSFalse;
    }
    
    JSString toJSString(Environment theEnv) {
        return new JSString(Double.toString(d));
    }
    
    JSObject toJSObject(Environment theEnv) {
        return new NativeNumber(d);
    }
    
    
 
    double d;
    
}