class ExpressionNode {

    ExpressionNode()
    {
    }
    
    String print(String indent)
    {
        return indent + "ExpressionNode(" + getClass().toString() + ")\n";
    }
    
    JSReference evalLHS(Environment theEnv)
    {
        System.out.println("Unimplemented evalLHS for " + print(""));
        return null;
    }
    
    JSValue eval(Environment theEnv)
    {
        System.out.println("Unimplemented eval for " + print(""));
        return null;
    }
    
}