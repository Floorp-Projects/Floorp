class AssignmentNode extends BinaryNode {
    
    AssignmentNode(String aOp, ExpressionNode aLeft, ExpressionNode aRight)
    {
        super(aOp, aLeft, aRight);
    }

    void eval(Environment theEnv)
    {
        left.evalLHS(theEnv);
        right.eval(theEnv);
        
        JSValue rValue = theEnv.theStack.pop();
        JSValue lValue = theEnv.theStack.pop();
        
        theEnv.theStack.push(rValue);
        lValue.putProp(theEnv);
    }

}