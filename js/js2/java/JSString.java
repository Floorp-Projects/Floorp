class JSString extends JSValue {
 
    JSString(String p)
    {
        s = p;
    }
    
    void eval(Environment theEnv)
    {
        theEnv.theStack.push(this);
    }
    
    void add(Environment theEnv)
    {
        JSString vR = theEnv.theStack.pop().toJSString();
        theEnv.theStack.push(new JSString(s + vR.s));
    }
    
    JSString toJSString()
    {
        return this;
    }
    
    String s;
    
}