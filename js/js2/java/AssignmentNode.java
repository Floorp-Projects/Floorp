class AssignmentNode extends BinaryNode {
    
    AssignmentNode(String aOp, ExpressionNode aLeft, ExpressionNode aRight)
    {
        super(aOp, aLeft, aRight);
    }

    void eval(Environment theEnv)
    {
        left.evalLHS(theEnv);
        right.eval(theEnv);
        
        double dValue = theEnv.theStack.pop().d;
        String id = theEnv.theStack.pop().id;
        
        theEnv.theGlobals.put(id, new Double(dValue));
        
        
    }

}