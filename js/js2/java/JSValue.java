class JSValue extends ExpressionNode {
    
    String print(String indent)
    {
        return indent + "JSValue\n";
    }

    void evalLHS(Environment theEnv)
    {
        throw new RuntimeException("EvalLHS on non-lvalue");
    }
    
    void eval(Environment theEnv)
    {
        throw new RuntimeException("Eval on JSValue");
    }    
    
    void unimplemented(String op)
    {
        throw new RuntimeException("unimplemented " + op + " called");
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
    
    void plus(Environment theEnv) {
        toJSDouble().plus(theEnv);
    }

    void minus(Environment theEnv) {
        toJSDouble().minus(theEnv);
    }

    void twiddle(Environment theEnv) {
        toJSInteger().twiddle(theEnv);
    }

    void bang(Environment theEnv) {
        toJSBoolean().bang(theEnv);
    }

    void typeof(Environment theEnv) {
        unimplemented("typeof");
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
    
    void multiply(Environment theEnv) {
        theEnv.theStack.push(theEnv.theStack.pop().toJSDouble());
        toJSDouble().multiply(theEnv);
    }
    
    void divide(Environment theEnv) {
        theEnv.theStack.push(theEnv.theStack.pop().toJSDouble());
        toJSDouble().divide(theEnv);
    }
    
    void remainder(Environment theEnv) {
        theEnv.theStack.push(theEnv.theStack.pop().toJSDouble());
        toJSDouble().remainder(theEnv);
    }
    
    void and(Environment theEnv) {
        theEnv.theStack.push(theEnv.theStack.pop().toJSInteger());
        toJSInteger().and(theEnv);
    }
    
    void or(Environment theEnv) {
        theEnv.theStack.push(theEnv.theStack.pop().toJSInteger());
        toJSInteger().or(theEnv);
    }
    
    void xor(Environment theEnv) {
        theEnv.theStack.push(theEnv.theStack.pop().toJSInteger());
        toJSInteger().xor(theEnv);
    }
    
    void shl(Environment theEnv) {
        theEnv.theStack.push(theEnv.theStack.pop().toJSInteger());
        toJSInteger().shl(theEnv);
    }
    
    void shr(Environment theEnv) {
        theEnv.theStack.push(theEnv.theStack.pop().toJSInteger());
        toJSInteger().shr(theEnv);
    }
    
    void ushl(Environment theEnv) {
        theEnv.theStack.push(theEnv.theStack.pop().toJSInteger());
        toJSInteger().ushl(theEnv);
    }     
    
    void getProp(Environment theEnv) {
        unimplemented("getProp"); 
    }
    
    void putProp(Environment theEnv) {
        unimplemented("putProp"); 
    }
    
    
    
    JSDouble toJSDouble()                               { unimplemented("toJSDouble"); return null; }
    JSInteger toJSInteger()                             { unimplemented("toJSInteger"); return null; }
    JSString toJSString()                               { unimplemented("toJSString"); return null; }
    JSBoolean toJSBoolean()                             { unimplemented("toJSBoolean"); return null; }
    JSValue toPrimitive(String hint)                    { unimplemented("toPrimitive"); return null; }        

}