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
        JSValue lV = toPrimitive(theEnv, "Number");
        JSValue rV = theEnv.theStack.pop().toPrimitive(theEnv, "Number");
        if ((lV instanceof JSString) && (rV instanceof JSString)) {
            theEnv.theStack.push(rV);
            lV.gt(theEnv);
        }
        else {
            theEnv.theStack.push(rV.toJSDouble(theEnv));
            lV.toJSDouble(theEnv).gt(theEnv);
        }
    }

    void ge(Environment theEnv) {
        JSValue lV = toPrimitive(theEnv, "Number");
        JSValue rV = theEnv.theStack.pop().toPrimitive(theEnv, "Number");
        if ((lV instanceof JSString) && (rV instanceof JSString)) {
            theEnv.theStack.push(rV);
            lV.ge(theEnv);
        }
        else {
            theEnv.theStack.push(rV.toJSDouble(theEnv));
            lV.toJSDouble(theEnv).ge(theEnv);
        }
    }

    void lt(Environment theEnv) {
        JSValue lV = toPrimitive(theEnv, "Number");
        JSValue rV = theEnv.theStack.pop().toPrimitive(theEnv, "Number");
        if ((lV instanceof JSString) && (rV instanceof JSString)) {
            theEnv.theStack.push(rV);
            lV.lt(theEnv);
        }
        else {
            theEnv.theStack.push(rV.toJSDouble(theEnv));
            lV.toJSDouble(theEnv).lt(theEnv);
        }
    }

    void le(Environment theEnv) {
        JSValue lV = toPrimitive(theEnv, "Number");
        JSValue rV = theEnv.theStack.pop().toPrimitive(theEnv, "Number");
        if ((lV instanceof JSString) && (rV instanceof JSString)) {
            theEnv.theStack.push(rV);
            lV.le(theEnv);
        }
        else {
            theEnv.theStack.push(rV.toJSDouble(theEnv));
            lV.toJSDouble(theEnv).le(theEnv);
        }
    }

    void eq(Environment theEnv) {
        JSValue lV = toPrimitive(theEnv, "Number");
        JSValue rV = theEnv.theStack.pop().toPrimitive(theEnv, "Number");
        if ((lV instanceof JSString) && (rV instanceof JSString)) {
            theEnv.theStack.push(rV);
            lV.eq(theEnv);
        }
        else {
            theEnv.theStack.push(rV.toJSDouble(theEnv));
            lV.toJSDouble(theEnv).eq(theEnv);
        }
    }

    void ne(Environment theEnv) {
        JSValue lV = toPrimitive(theEnv, "Number");
        JSValue rV = theEnv.theStack.pop().toPrimitive(theEnv, "Number");
        if ((lV instanceof JSString) && (rV instanceof JSString)) {
            theEnv.theStack.push(rV);
            lV.ne(theEnv);
        }
        else {
            theEnv.theStack.push(rV.toJSDouble(theEnv));
            lV.toJSDouble(theEnv).ne(theEnv);
        }
    }
    
    void plus(Environment theEnv) {
        toJSDouble(theEnv).plus(theEnv);
    }

    void minus(Environment theEnv) {
        toJSDouble(theEnv).minus(theEnv);
    }

    void twiddle(Environment theEnv) {
        toJSInteger(theEnv).twiddle(theEnv);
    }

    void bang(Environment theEnv) {
        toJSBoolean(theEnv).bang(theEnv);
    }

    void typeof(Environment theEnv) {
        unimplemented("typeof");
    }

    void add(Environment theEnv) {
        JSValue lV = toPrimitive(theEnv, "");
        JSValue rV = theEnv.theStack.pop().toPrimitive(theEnv, "");
        if ((lV instanceof JSString) || (rV instanceof JSString)) {
            theEnv.theStack.push(rV);
            lV.ne(theEnv);
        }
        else {
            theEnv.theStack.push(rV.toJSDouble(theEnv));
            lV.toJSDouble(theEnv).add(theEnv);
        }
    }
    
    void subtract(Environment theEnv) {
        theEnv.theStack.push(theEnv.theStack.pop().toJSDouble(theEnv));
        toJSDouble(theEnv).subtract(theEnv);
    }
    
    void multiply(Environment theEnv) {
        theEnv.theStack.push(theEnv.theStack.pop().toJSDouble(theEnv));
        toJSDouble(theEnv).multiply(theEnv);
    }
    
    void divide(Environment theEnv) {
        theEnv.theStack.push(theEnv.theStack.pop().toJSDouble(theEnv));
        toJSDouble(theEnv).divide(theEnv);
    }
    
    void remainder(Environment theEnv) {
        theEnv.theStack.push(theEnv.theStack.pop().toJSDouble(theEnv));
        toJSDouble(theEnv).remainder(theEnv);
    }
    
    void and(Environment theEnv) {
        theEnv.theStack.push(theEnv.theStack.pop().toJSInteger(theEnv));
        toJSInteger(theEnv).and(theEnv);
    }
    
    void or(Environment theEnv) {
        theEnv.theStack.push(theEnv.theStack.pop().toJSInteger(theEnv));
        toJSInteger(theEnv).or(theEnv);
    }
    
    void xor(Environment theEnv) {
        theEnv.theStack.push(theEnv.theStack.pop().toJSInteger(theEnv));
        toJSInteger(theEnv).xor(theEnv);
    }
    
    void shl(Environment theEnv) {
        theEnv.theStack.push(theEnv.theStack.pop().toJSInteger(theEnv));
        toJSInteger(theEnv).shl(theEnv);
    }
    
    void shr(Environment theEnv) {
        theEnv.theStack.push(theEnv.theStack.pop().toJSInteger(theEnv));
        toJSInteger(theEnv).shr(theEnv);
    }
    
    void ushl(Environment theEnv) {
        theEnv.theStack.push(theEnv.theStack.pop().toJSInteger(theEnv));
        toJSInteger(theEnv).ushl(theEnv);
    }     
    
    void getProp(Environment theEnv) {
        toJSObject(theEnv).getProp(theEnv);
    }
    
    void putProp(Environment theEnv) {
        toJSObject(theEnv).putProp(theEnv);
    }
    
    JSValue defaultValue(Environment theEnv, String hint) {
/*
When the [[DefaultValue]] method of O is called with hint String, the following steps are taken:
1. Call the [[Get]] method of object O with argument "toString".
2. If Result(1) is not an object, go to step 5.
3. Call the [[Call]] method of Result(1), with O as the this value and an empty argument list.
4. If Result(3) is a primitive value, return Result(3).
5. Call the [[Get]] method of object O with argument "valueOf".
6. If Result(5) is not an object, go to step 9.
7. Call the [[Call]] method of Result(5), with O as the this value and an empty argument list.
8. If Result(7) is a primitive value, return Result(7).
9. Generate a runtime error.
*/
        JSValue v = null;
        if (hint.equals("String")) {
            theEnv.theStack.push(new JSString("toString"));
            getProp(theEnv);
            v = theEnv.theStack.pop();
            if (v instanceof JSObject) {
                // invoke 'v.Call' with 'this' as the JS this
            }
            else {
                theEnv.theStack.push(new JSString("valueOf"));
                getProp(theEnv);
                v = theEnv.theStack.pop();
                if (v instanceof JSObject) {                    
                }
                else 
                    throw new JSException(new JSString("No default value"));
            }
        }
        else { // hint.equals("Number")
/*
When the [[DefaultValue]] method of O is called with hint Number, the following steps are taken:
1. Call the [[Get]] method of object O with argument "valueOf".
2. If Result(1) is not an object, go to step 5.
3. Call the [[Call]] method of Result(1), with O as the this value and an empty argument list.
4. If Result(3) is a primitive value, return Result(3).
5. Call the [[Get]] method of object O with argument "toString".
6. If Result(5) is not an object, go to step 9.
7. Call the [[Call]] method of Result(5), with O as the this value and an empty argument list.
8. If Result(7) is a primitive value, return Result(7).
9. Generate a runtime error.
*/
            
        }
        return null;
    }
    
    JSValue toPrimitive(Environment theEnv, String hint) {
        JSValue result = defaultValue(theEnv, hint);
        if (result instanceof JSObject)
            throw new JSException(new JSString("default value returned object"));
        else
            return result;
    }
       
    
    JSObject toJSObject(Environment theEnv)       { unimplemented("toJSObjet"); return null; }
    JSDouble toJSDouble(Environment theEnv)       { unimplemented("toJSDouble"); return null; }
    JSInteger toJSInteger(Environment theEnv)     { unimplemented("toJSInteger"); return null; }
    JSString toJSString(Environment theEnv)       { unimplemented("toJSString"); return null; }
    JSBoolean toJSBoolean(Environment theEnv)     { unimplemented("toJSBoolean"); return null; }

}