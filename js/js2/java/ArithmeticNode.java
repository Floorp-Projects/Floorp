class ArithmeticNode extends BinaryNode {

    ArithmeticNode(String aOp, ExpressionNode aLeft, ExpressionNode aRight)
    {
        super(aOp, aLeft, aRight);
    }

    void eval(Environment theEnv)
    {
        super.eval(theEnv);
        double dR = theEnv.theStack.pop().d;
        double dL = theEnv.theStack.pop().d;
        if (op == "+")
            theEnv.theStack.push(new StackValue(dL + dR));
        else
        if (op == "-")
            theEnv.theStack.push(new StackValue(dL - dR));
        else
            System.out.println("missing arithmetic op " + op);
    }

}