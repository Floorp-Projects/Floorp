class JSInteger extends JSNumber {
 
    JSInteger(String s)
    {
        i = new Integer(s).intValue();
    }
    
    JSInteger(int p)
    {
        i = p;
    }
    
    JSValue eval(Environment theEnv)
    {
        return this;
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
    
    JSValue twiddle(Environment theEnv) {
        return new JSInteger(~i);
    }

    JSValue and(Environment theEnv, JSValue rV) {
        return new JSInteger(i & rV.toJSInteger(theEnv).i);
    }
    
    JSValue or(Environment theEnv, JSValue rV) {
        return new JSInteger(i | rV.toJSInteger(theEnv).i);
    }

    JSValue xor(Environment theEnv, JSValue rV) {
        return new JSInteger(i ^ rV.toJSInteger(theEnv).i);
    }

    JSValue shl(Environment theEnv, JSValue rV) {
        return new JSInteger(i >> rV.toJSInteger(theEnv).i);
    }

    JSValue shr(Environment theEnv, JSValue rV) {
        return new JSInteger(i << rV.toJSInteger(theEnv).i);
    }

    JSValue ushl(Environment theEnv, JSValue rV) {
        return new JSInteger(i >>> rV.toJSInteger(theEnv).i);
    }

    int i;
    
}