class BitwiseNode extends BinaryNode {

    BitwiseNode(String aOp, ExpressionNode aLeft, ExpressionNode aRight)
    {
        super(aOp, aLeft, aRight);
    }

    void eval(Environment theEnv)
    {
        super.eval(theEnv);
        int iR = (int)(theEnv.theStack.pop().d);
        int iL = (int)(theEnv.theStack.pop().d);
        if (op == "&")
            theEnv.theStack.push(new StackValue(iL & iR));
        else
        if (op == "|")
            theEnv.theStack.push(new StackValue(iL | iR));
        else
        if (op == "^")
            theEnv.theStack.push(new StackValue(iL ^ iR));
        else
        if (op == "<<")
            theEnv.theStack.push(new StackValue(iL << iR));
        else
        if (op == ">>")
            theEnv.theStack.push(new StackValue(iL >> iR));
        else
        if (op == ">>>")
            theEnv.theStack.push(new StackValue(iL >>> iR));
        else
            System.out.println("missing bitwise op " + op);
    }

}
