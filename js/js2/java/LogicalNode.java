class LogicalNode extends BinaryNode {

    LogicalNode(String aOp, ExpressionNode aLeft, ExpressionNode aRight)
    {
        super(aOp, aLeft, aRight);
    }

    void eval(Environment theEnv)
    {
        left.eval(theEnv);
        JSBoolean b = theEnv.theStack.pop().toJSBoolean();
        if (op == "&&") {
            if (b.isFalse())
                theEnv.theStack.push(b);
            else {
                right.eval(theEnv);
                b = theEnv.theStack.pop().toJSBoolean();
                if (b.isFalse())
                    theEnv.theStack.push(b);
                else
                    theEnv.theStack.push(JSBoolean.JSTrue);
            }
        }
        if (op == "||") {
            if (b.isTrue())
                theEnv.theStack.push(b);
            else {
                right.eval(theEnv);
                b = theEnv.theStack.pop().toJSBoolean();
                if (b.isTrue())
                    theEnv.theStack.push(b);
                else
                    theEnv.theStack.push(JSBoolean.JSFalse);
            }
        }
        else
            System.out.println("missing logical op " + op);
    }

}
