class BinaryNode extends ExpressionNode {
    
    BinaryNode(String aOp, ExpressionNode aLeft, ExpressionNode aRight)
    {
        left = aLeft;
        right = aRight;
        op = aOp;
    }

    void evalLHS(Environment theEnv)
    {
        if (op == ".") {
            left.eval(theEnv);
            JSValue lV = theEnv.theStack.pop();
            if (right instanceof JSIdentifier)
                theEnv.theStack.push((JSValue)right);
            else
                right.eval(theEnv);
            theEnv.theStack.push(lV);
        }
        else
            throw new RuntimeException("bad lValue operator " + op);
    }
    
    void eval(Environment theEnv)
    {
        left.eval(theEnv);
        JSValue lV = theEnv.theStack.pop();
        right.eval(theEnv);
        
        if (op == ".")
            lV.getProp(theEnv);
        else
            System.out.println("missing binary op " + op);
        
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