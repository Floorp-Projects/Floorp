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

    void eval(Environment theEnv)
    {
        theEnv.theStack.push(this);
    }

    
    void add(Environment theEnv) {
        JSValue vR = theEnv.theStack.peek();
        if (vR instanceof JSString)         
            toJSString().add(theEnv);
        else {
            theEnv.theStack.pop();
            theEnv.theStack.push(new JSDouble(d + vR.toJSDouble().d));
        }
    }
    
    void subtract(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push(new JSDouble(d - vR.toJSDouble().d));
    }
    
    void multiply(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push(new JSDouble(d * vR.toJSDouble().d));
    }

    void divide(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push(new JSDouble(d / vR.toJSDouble().d));
    }
    
    void remainder(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push(new JSDouble(d % vR.toJSDouble().d));
    }
    
    void gt(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push((d > vR.toJSDouble().d) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
    }

    void ge(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push((d >= vR.toJSDouble().d) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
    }
    
    void lt(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push((d < vR.toJSDouble().d) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
    }
    
    void le(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push((d <= vR.toJSDouble().d) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
    }
    
    void eq(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push((d == vR.toJSDouble().d) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
    }
    
    void ne(Environment theEnv) {
        JSValue vR = theEnv.theStack.pop();
        theEnv.theStack.push((d != vR.toJSDouble().d) ? JSBoolean.JSTrue : JSBoolean.JSFalse);
    }
    
    JSDouble toJSDouble() {
        return this;
    }
    
    JSInteger toJSInteger() {
        return new JSInteger((int)d);
    }
    
    JSBoolean toJSBoolean() {
        return (d != 0.0) ? JSBoolean.JSTrue : JSBoolean.JSFalse;
    }
    
    JSString toJSString() {
        return new JSString(Double.toString(d));
    }
 
    public String toString() {
        return Double.toString(d);
    }
    
    double d;
    
}