class JSValue extends ExpressionNode {

    JSValue(String aType, String aValue)
    {
        type = aType;
        value = aValue;
    }
    
    String print(String indent)
    {
        return indent + "JSValue " + type + " : " + value + "\n";
    }
    
    String type;
    String value;

}