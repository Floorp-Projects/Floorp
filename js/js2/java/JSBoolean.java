class JSBoolean extends JSValue {
 
    JSBoolean(String s)
    {
        if (s.equals("true"))        
            b = true;
        else
            if (s.equals("false"))        
                b = false;
            else
                throw new RuntimeException("Bad string for JSBoolean constructor : " + s);
    }
    
    void eval(Environment theEnv)
    {
        theEnv.theStack.push(new StackValue(b ? 1 : 0));
    }
    
    boolean b;
    
}