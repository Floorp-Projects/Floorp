class JSObject extends JSValue {
    
    static JSObject JSUndefined = new JSObject("undefined");
    
    JSObject(String aValue)
    {
        value = aValue;
    }
    
    String print(String indent)
    {
        return indent + "JSObject : " + value + "\n";
    }

    void evalLHS(Environment theEnv)
    {
        theEnv.theStack.push(this);
    }
    
    void eval(Environment theEnv)
    {
        JSValue v = (JSValue)(theEnv.theGlobals.get(value));
        if (v == null) {
            System.out.println("Accessed undefined : " + value);
            theEnv.theStack.push(JSUndefined);
        }
        else
            theEnv.theStack.push(v);
    }
    
    JSValue defaultValue(String hint) {
        return null;                    // XXX
    }
    
    JSValue toPrimitive(String hint) {
        JSValue result = defaultValue(hint);
        if (result instanceof JSObject)
            throw new JSException(new JSString("default value returned object"));
        else
            return result;
    }
    
    JSBoolean toJSBoolean() {
        return JSBoolean.JSTrue;
    }
        
    JSDouble toJSDouble() {
        return toPrimitive("Number").toJSDouble();
    }
        
    void gt(Environment theEnv) {
        JSValue lV = toPrimitive("Number");
        JSValue rV = theEnv.theStack.pop().toPrimitive("Number");
        if ((lV instanceof JSString) && (rV instanceof JSString)) {
            theEnv.theStack.push(rV);
            lV.gt(theEnv);
        }
        else {
            theEnv.theStack.push(rV.toJSDouble());
            lV.toJSDouble().gt(theEnv);
        }
    }

    void ge(Environment theEnv) {
        JSValue lV = toPrimitive("Number");
        JSValue rV = theEnv.theStack.pop().toPrimitive("Number");
        if ((lV instanceof JSString) && (rV instanceof JSString)) {
            theEnv.theStack.push(rV);
            lV.ge(theEnv);
        }
        else {
            theEnv.theStack.push(rV.toJSDouble());
            lV.toJSDouble().ge(theEnv);
        }
    }

    void lt(Environment theEnv) {
        JSValue lV = toPrimitive("Number");
        JSValue rV = theEnv.theStack.pop().toPrimitive("Number");
        if ((lV instanceof JSString) && (rV instanceof JSString)) {
            theEnv.theStack.push(rV);
            lV.lt(theEnv);
        }
        else {
            theEnv.theStack.push(rV.toJSDouble());
            lV.toJSDouble().lt(theEnv);
        }
    }

    void le(Environment theEnv) {
        JSValue lV = toPrimitive("Number");
        JSValue rV = theEnv.theStack.pop().toPrimitive("Number");
        if ((lV instanceof JSString) && (rV instanceof JSString)) {
            theEnv.theStack.push(rV);
            lV.le(theEnv);
        }
        else {
            theEnv.theStack.push(rV.toJSDouble());
            lV.toJSDouble().le(theEnv);
        }
    }

    void eq(Environment theEnv) {
        JSValue lV = toPrimitive("Number");
        JSValue rV = theEnv.theStack.pop().toPrimitive("Number");
        if ((lV instanceof JSString) && (rV instanceof JSString)) {
            theEnv.theStack.push(rV);
            lV.eq(theEnv);
        }
        else {
            theEnv.theStack.push(rV.toJSDouble());
            lV.toJSDouble().eq(theEnv);
        }
    }

    void ne(Environment theEnv) {
        JSValue lV = toPrimitive("Number");
        JSValue rV = theEnv.theStack.pop().toPrimitive("Number");
        if ((lV instanceof JSString) && (rV instanceof JSString)) {
            theEnv.theStack.push(rV);
            lV.ne(theEnv);
        }
        else {
            theEnv.theStack.push(rV.toJSDouble());
            lV.toJSDouble().ne(theEnv);
        }
    }

    void add(Environment theEnv) {
        JSValue lV = toPrimitive("");
        JSValue rV = theEnv.theStack.pop().toPrimitive("");
        if ((lV instanceof JSString) || (rV instanceof JSString)) {
            theEnv.theStack.push(rV);
            lV.ne(theEnv);
        }
        else {
            theEnv.theStack.push(rV.toJSDouble());
            lV.toJSDouble().add(theEnv);
        }
    }
    
    void subtract(Environment theEnv) {
        theEnv.theStack.push(theEnv.theStack.pop().toJSDouble());
        toJSDouble().subtract(theEnv);
    }
    
    
    public String toString() {
        return value;
    }
        
    String value;
    
}