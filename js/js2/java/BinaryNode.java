class BinaryNode extends ExpressionNode {
    
    BinaryNode(String aOp, ExpressionNode aLeft, ExpressionNode aRight)
    {
        left = aLeft;
        right = aRight;
        op = aOp;
    }
    
    String print(String indent)
    {
        StringBuffer result = new StringBuffer(indent);
        result.append(getClass().toString());
        result.append(" ");
        result.append(op);
        result.append("\n");
        indent += "    ";
        if (left == null) {
            result.append(indent);
            result.append("null\n");
        }
        else
            result.append(left.print(indent));
        if (right == null) {
            result.append(indent);
            result.append("null\n");
        }
        else
            result.append(right.print(indent));
        return result.toString();
    }
    
    protected ExpressionNode left;
    protected ExpressionNode right;
    protected String op;
    
}