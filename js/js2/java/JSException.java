

class JSException extends RuntimeException {

    JSException(StackValue x)
    {
        value = x;
    }
    
    StackValue getValue()
    {
        return value;
    }
    
    StackValue value;
    
}