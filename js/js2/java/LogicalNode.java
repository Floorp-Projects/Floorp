class LogicalNode extends BinaryNode {

    LogicalNode(String aOp, ExpressionNode aLeft, ExpressionNode aRight)
    {
        super(aOp, aLeft, aRight);
    }

    void eval(Environment theEnv)
    {
        left.eval(theEnv);
        double d = theEnv.theStack.pop().d;
        if (op == "&&") {
            if (d == 0.0)
                theEnv.theStack.push(new StackValue(0));
            else {
                right.eval(theEnv);
                d = theEnv.theStack.pop().d;
                if (d == 0.0)
                    theEnv.theStack.push(new StackValue(0));
                else
                    theEnv.theStack.push(new StackValue(1));
            }
        }
        if (op == "||") {
            if (d != 0.0)
                theEnv.theStack.push(new StackValue(1));
            else {
                right.eval(theEnv);
                d = theEnv.theStack.pop().d;
                if (d != 0.0)
                    theEnv.theStack.push(new StackValue(1));
                else
                    theEnv.theStack.push(new StackValue(0));
            }
        }
        else
            System.out.println("missing logical op " + op);
    }

}
