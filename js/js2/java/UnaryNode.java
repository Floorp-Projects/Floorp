class UnaryNode extends ExpressionNode {
    
    UnaryNode(String aOp, ExpressionNode aChild)
    {
        child = aChild;
        op = aOp;
    }
    
    String print(String indent)
    {
        StringBuffer result = new StringBuffer(indent);
        result.append("UnaryNode ");
        result.append(op);
        result.append("\n");
        indent += "    ";
        if (child == null) {
            result.append(indent);
            result.append("null\n");
        }
        else
            result.append(child.print(indent));
        return result.toString();
    }
    
    void eval(Environment theEnv)
    {
        child.eval(theEnv);
        JSValue cV = theEnv.theStack.pop();

        if (op == "+")
            cV.plus(theEnv);
        else
        if (op == "-")
            cV.minus(theEnv);
        else
        if (op == "~")
            cV.twiddle(theEnv);
        else
        if (op == "!")
            cV.bang(theEnv);
        else
        if (op == "typeof")
            cV.typeof(theEnv);
        else
            System.out.println("missing unary op " + op);
    }

    String getOperator()
    {
        return op;
    }
    
    ExpressionNode getChild()
    {
        return child;
    }
    
    protected ExpressionNode child;
    protected String op;
    
}