class BinaryNode extends ExpressionNode {
    
    BinaryNode(String aOp, ExpressionNode aLeft, ExpressionNode aRight)
    {
        left = aLeft;
        right = aRight;
        op = aOp;
    }

    JSReference evalLHS(Environment theEnv)
    {
        if (op == ".") {
            JSValue lV = left.eval(theEnv);
            JSString id;
            if (right instanceof JSIdentifier)
                id = (JSString)right;
            else
                id = right.eval(theEnv).toJSString(theEnv);
            return new JSReference(lV, id);
        }
        else
            throw new RuntimeException("bad lValue operator " + op);
    }
    
    JSValue eval(Environment theEnv)
    {
        JSValue lV = left.eval(theEnv);
        JSValue rV = right.eval(theEnv);
        
        if (op == ".")
            return lV.getProp(theEnv, rV.toJSString(theEnv));
        else
        if (op == "()")
            return lV.call(theEnv, rV);
        else
        if (op == ",")
            return JSValueList.buildList(lV, rV);
        else {
            System.out.println("missing binary op " + op);
            return null;
        }
        
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