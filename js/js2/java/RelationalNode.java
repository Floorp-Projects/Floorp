class RelationalNode extends BinaryNode {

    RelationalNode(String aOp, ExpressionNode aLeft, ExpressionNode aRight)
    {
        super(aOp, aLeft, aRight);
    }

    void eval(Environment theEnv)
    {
        super.eval(theEnv);
        double dR = theEnv.theStack.pop().d;
        double dL = theEnv.theStack.pop().d;
        if (op == ">")
            theEnv.theStack.push(new StackValue((dL > dR) ? 1 : 0));
        else
        if (op == ">=")
            theEnv.theStack.push(new StackValue((dL >= dR) ? 1 : 0));
        else
        if (op == "<")
            theEnv.theStack.push(new StackValue((dL < dR) ? 1 : 0));
        else
        if (op == "<=")
            theEnv.theStack.push(new StackValue((dL <= dR) ? 1 : 0));
        else
        if (op == "==")
            theEnv.theStack.push(new StackValue((dL == dR) ? 1 : 0));
        else
        if (op == "!=")
            theEnv.theStack.push(new StackValue((dL != dR) ? 1 : 0));
        else
            System.out.println("missing relational op");
    }
}
