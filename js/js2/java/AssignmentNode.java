class AssignmentNode extends BinaryNode {
    
    AssignmentNode(String aOp, ExpressionNode aLeft, ExpressionNode aRight)
    {
        super(aOp, aLeft, aRight);
    }

    JSValue eval(Environment theEnv)
    {
        JSReference lV = left.evalLHS(theEnv);
        JSValue rV = right.eval(theEnv);
        
        return lV.base.putProp(theEnv, lV.id, rV);
    }

}