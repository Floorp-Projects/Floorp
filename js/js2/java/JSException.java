

class JSException extends RuntimeException {

    JSException(JSValue x)
    {
        value = x;
    }
    
    JSValue getValue()
    {
        return value;
    }
    
    public String toString()
    {
        return value.toJSString(null).s;
    }
    
    JSValue value;
    
}