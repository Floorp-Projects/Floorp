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