class JSDouble extends JSNumber {
 
    JSDouble(String s)
    {
        d = new Double(s).doubleValue();
    }
    
    String print(String indent)
    {
        return indent + "JSDouble " + d + "\n";
    }

    void eval(Environment theEnv)
    {
        theEnv.theStack.push(new StackValue(d));
    }
    
    double d;
    
}