class JSString extends JSValue {
 
    JSString(String p)
    {
        s = p;
    }
    
    void eval(Environment theEnv)
    {
        theEnv.theStack.push(new StackValue(s));
    }
    
    
    String s;
    
}