class ExpressionNode {

    ExpressionNode()
    {
    }
    
    String print(String indent)
    {
        return indent + "ExpressionNode(" + getClass().toString() + ")\n";
    }
    
    void evalLHS(Environment theEnv)
    {
        System.out.println("Unimplemented evalLHS for " + print(""));
    }
    
    void eval(Environment theEnv)
    {
        System.out.println("Unimplemented eval for " + print(""));
    }
    
}