class JSObject extends JSValue {
    
    JSObject(String aType, String aValue)
    {
        type = aType;
        value = aValue;
    }
    
    String print(String indent)
    {
        return indent + "JSObject " + type + " : " + value + "\n";
    }

    void evalLHS(Environment theEnv)
    {
        if (type == "object") {
            theEnv.theStack.push(new StackValue(value));
        }
        else {
            System.out.println("EvalLHS on non-object");
        }
    }
    
    void eval(Environment theEnv)
    {
        if (type.equals("object")) {
            Double d = (Double)(theEnv.theGlobals.get(value));
            if (d == null) {
                System.out.println("Accessed undefined : " + value);
                theEnv.theStack.push(new StackValue(0.0));
            }
            else
                theEnv.theStack.push(new StackValue(d.doubleValue()));
        }
    }    
    
    String type;
    String value;
    
}