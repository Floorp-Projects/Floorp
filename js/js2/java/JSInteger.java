class JSInteger extends JSNumber {
 
    JSInteger(String s)
    {
        i = new Integer(s).intValue();
    }
    
    void eval(Environment theEnv)
    {
        theEnv.theStack.push(new StackValue(i));
    }
    
    int i;
    
}