class LogicalNode extends BinaryNode {

    LogicalNode(String aOp, ExpressionNode aLeft, ExpressionNode aRight)
    {
        super(aOp, aLeft, aRight);
    }

    JSValue eval(Environment theEnv)
    {
        JSBoolean b = left.eval(theEnv).toJSBoolean(theEnv);
        if (op == "&&") {
            if (b.isFalse())
                return b;
            else {
                b = right.eval(theEnv).toJSBoolean(theEnv);
                if (b.isFalse())
                    return b;
                else
                    return JSBoolean.JSTrue;
            }
        }
        if (op == "||") {
            if (b.isTrue())
                return b;
            else {
                b = right.eval(theEnv).toJSBoolean(theEnv);
                if (b.isTrue())
                    return b;
                else
                    return JSBoolean.JSFalse;
            }
        }
        else {
            System.out.println("missing logical op " + op);
            return null;
        }
    }

}
