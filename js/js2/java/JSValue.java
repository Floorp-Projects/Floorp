class JSValue extends ExpressionNode {
    
    String print(String indent)
    {
        return indent + "JSValue\n";
    }

    JSReference evalLHS(Environment theEnv)
    {
        throw new RuntimeException("EvalLHS on non-lvalue");
    }
    
    JSValue eval(Environment theEnv)
    {
        throw new RuntimeException("Eval on JSValue");
    }    
    
    JSValue unimplemented(String op)
    {
        throw new RuntimeException("unimplemented " + op + " called");
    }
    
    JSValue gt(Environment theEnv, JSValue rV) {
        JSValue lV = toPrimitive(theEnv, "Number");
        rV = rV.toPrimitive(theEnv, "Number");
        if ((lV instanceof JSString) && (rV instanceof JSString))
            return lV.gt(theEnv, rV);
        else
            return lV.toJSDouble(theEnv).gt(theEnv, rV.toJSDouble(theEnv));
    }

    JSValue ge(Environment theEnv, JSValue rV) {
        JSValue lV = toPrimitive(theEnv, "Number");
        rV = rV.toPrimitive(theEnv, "Number");
        if ((lV instanceof JSString) && (rV instanceof JSString))
            return lV.ge(theEnv, rV);
        else
            return lV.toJSDouble(theEnv).ge(theEnv, rV.toJSDouble(theEnv));
    }

    JSValue lt(Environment theEnv, JSValue rV) {
        JSValue lV = toPrimitive(theEnv, "Number");
        rV = rV.toPrimitive(theEnv, "Number");
        if ((lV instanceof JSString) && (rV instanceof JSString))
            return lV.lt(theEnv, rV);
        else
            return lV.toJSDouble(theEnv).lt(theEnv, rV.toJSDouble(theEnv));
    }

    JSValue le(Environment theEnv, JSValue rV) {
        JSValue lV = toPrimitive(theEnv, "Number");
        rV = rV.toPrimitive(theEnv, "Number");
        if ((lV instanceof JSString) && (rV instanceof JSString))
            return lV.le(theEnv, rV);
        else
            return lV.toJSDouble(theEnv).le(theEnv, rV.toJSDouble(theEnv));
    }

    JSValue eq(Environment theEnv, JSValue rV) {
        JSValue lV = toPrimitive(theEnv, "Number");
        rV = rV.toPrimitive(theEnv, "Number");
        if ((lV instanceof JSString) && (rV instanceof JSString))
            return lV.eq(theEnv, rV);
        else
            return lV.toJSDouble(theEnv).eq(theEnv, rV.toJSDouble(theEnv));
    }

    JSValue ne(Environment theEnv, JSValue rV) {
        JSValue lV = toPrimitive(theEnv, "Number");
        rV = rV.toPrimitive(theEnv, "Number");
        if ((lV instanceof JSString) && (rV instanceof JSString))
            return lV.ne(theEnv, rV);
        else
            return lV.toJSDouble(theEnv).ne(theEnv, rV.toJSDouble(theEnv));
    }
    
    JSValue plus(Environment theEnv) {
        return toJSDouble(theEnv).plus(theEnv);
    }

    JSValue minus(Environment theEnv) {
        return toJSDouble(theEnv).minus(theEnv);
    }

    JSValue twiddle(Environment theEnv) {
        return toJSInteger(theEnv).twiddle(theEnv);
    }

    JSValue bang(Environment theEnv) {
        return toJSBoolean(theEnv).bang(theEnv);
    }

    JSValue typeof(Environment theEnv) {
        return unimplemented("typeof");
    }

    JSValue add(Environment theEnv, JSValue rV) {
        JSValue lV = toPrimitive(theEnv, "");
        rV = rV.toPrimitive(theEnv, "");
        if ((lV instanceof JSString) || (rV instanceof JSString))
            return lV.add(theEnv, rV);
        else
            return lV.toJSDouble(theEnv).add(theEnv, rV);
    }
    
    JSValue subtract(Environment theEnv, JSValue rV) {
        return toJSDouble(theEnv).subtract(theEnv, rV.toJSDouble(theEnv));
    }
    
    JSValue multiply(Environment theEnv, JSValue rV) {
        return toJSDouble(theEnv).multiply(theEnv, rV.toJSDouble(theEnv));
    }
    
    JSValue divide(Environment theEnv, JSValue rV) {
        return toJSDouble(theEnv).divide(theEnv, rV.toJSDouble(theEnv));
    }
    
    JSValue remainder(Environment theEnv, JSValue rV) {
        return toJSDouble(theEnv).remainder(theEnv, rV.toJSDouble(theEnv));
    }
    
    JSValue and(Environment theEnv, JSValue rV) {
        return toJSInteger(theEnv).and(theEnv, rV.toJSInteger(theEnv));
    }
    
    JSValue or(Environment theEnv, JSValue rV) {
        return toJSInteger(theEnv).or(theEnv, rV.toJSInteger(theEnv));
    }
    
    JSValue xor(Environment theEnv, JSValue rV) {
        return toJSInteger(theEnv).xor(theEnv, rV.toJSInteger(theEnv));
    }
    
    JSValue shl(Environment theEnv, JSValue rV) {
        return toJSInteger(theEnv).shl(theEnv, rV.toJSInteger(theEnv));
    }
    
    JSValue shr(Environment theEnv, JSValue rV) {
        return toJSInteger(theEnv).shr(theEnv, rV.toJSInteger(theEnv));
    }
    
    JSValue ushl(Environment theEnv, JSValue rV) {
        return toJSInteger(theEnv).ushl(theEnv, rV.toJSInteger(theEnv));
    }     
    
    JSValue getProp(Environment theEnv, JSString id) {
        return toJSObject(theEnv).getProp(theEnv, id);
    }
    
    JSValue putProp(Environment theEnv, JSString id, JSValue rV) {
        return toJSObject(theEnv).putProp(theEnv, id, rV);
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
            v = getProp(theEnv, new JSString("toString"));
            if (v instanceof JSObject) {
                // invoke 'v.Call' with 'this' as the JS this
            }
            else {
                v = getProp(theEnv, new JSString("valueOf"));
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