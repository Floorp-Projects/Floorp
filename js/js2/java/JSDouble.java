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
    
    JSValue eval(Environment theEnv)
    {
        return this;
    }

    JSValue plus(Environment theEnv) {
        return this;
    }
    
    JSValue minus(Environment theEnv) {
        return new JSDouble(-d);
    }
    
    JSValue add(Environment theEnv, JSValue rV) {
        if (rV instanceof JSString)         
            return toJSString(theEnv).add(theEnv, rV);
        else
            return new JSDouble(d + rV.toJSDouble(theEnv).d);
    }
    
    JSValue subtract(Environment theEnv, JSValue rV) {
        return new JSDouble(d - rV.toJSDouble(theEnv).d);
    }
    
    JSValue multiply(Environment theEnv, JSValue rV) {
        return new JSDouble(d * rV.toJSDouble(theEnv).d);
    }

    JSValue divide(Environment theEnv, JSValue rV) {
        return new JSDouble(d / rV.toJSDouble(theEnv).d);
    }
    
    JSValue remainder(Environment theEnv, JSValue rV) {
        return new JSDouble(d % rV.toJSDouble(theEnv).d);
    }
    
    JSValue gt(Environment theEnv, JSValue rV) {
        return (d > rV.toJSDouble(theEnv).d) ? JSBoolean.JSTrue : JSBoolean.JSFalse;
    }

    JSValue ge(Environment theEnv, JSValue rV) {
        return (d >= rV.toJSDouble(theEnv).d) ? JSBoolean.JSTrue : JSBoolean.JSFalse;
    }
    
    JSValue lt(Environment theEnv, JSValue rV) {
        return (d < rV.toJSDouble(theEnv).d) ? JSBoolean.JSTrue : JSBoolean.JSFalse;
    }
    
    JSValue le(Environment theEnv, JSValue rV) {
        return (d <= rV.toJSDouble(theEnv).d) ? JSBoolean.JSTrue : JSBoolean.JSFalse;
    }
    
    JSValue eq(Environment theEnv, JSValue rV) {
        return (d == rV.toJSDouble(theEnv).d) ? JSBoolean.JSTrue : JSBoolean.JSFalse;
    }
    
    JSValue ne(Environment theEnv, JSValue rV) {
        return (d != rV.toJSDouble(theEnv).d) ? JSBoolean.JSTrue : JSBoolean.JSFalse;
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