class JSValue extends ExpressionNode {
    
    String print(String indent)
    {
        return indent + "JSValue\n";
    }

    void evalLHS(Environment theEnv)
    {
        throw new RuntimeException("EvalLHS on non-lvalue");
    }
    
    void eval(Environment theEnv)
    {
        throw new RuntimeException("Eval on JSValue");
    }


}