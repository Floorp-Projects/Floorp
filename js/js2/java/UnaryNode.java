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
    
    JSValue eval(Environment theEnv)
    {
        JSValue cV = child.eval(theEnv);

        if (op == "+")
            return cV.plus(theEnv);
        else
        if (op == "-")
            return cV.minus(theEnv);
        else
        if (op == "~")
            return cV.twiddle(theEnv);
        else
        if (op == "!")
            return cV.bang(theEnv);
        else
        if (op == "typeof")
            return cV.typeof(theEnv);
        else {
            System.out.println("missing unary op " + op);
            return null;
        }
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