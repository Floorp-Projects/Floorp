class NativeFunction extends JSObject {
    
    NativeFunction(ControlNode aBody)
    {
        super("Function", null);
        body = aBody;
    }
    
    JSValue call(Environment theEnv, JSValue rV)
    {
        ControlNode c = body;
        while (c != null) c = c.eval(theEnv);
        
        return theEnv.resultValue;
    }
    
    ControlNode body;
    
}