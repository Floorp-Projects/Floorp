

class JSException extends RuntimeException {

    JSException(JSValue x)
    {
        value = x;
    }
    
    JSValue getValue()
    {
        return value;
    }
    
    JSValue value;
    
}