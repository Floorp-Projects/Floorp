class BitwiseNode extends BinaryNode {

    BitwiseNode(String aOp, ExpressionNode aLeft, ExpressionNode aRight)
    {
        super(aOp, aLeft, aRight);
    }

    void eval(Environment theEnv)
    {
        left.eval(theEnv);
        JSInteger lV = theEnv.theStack.pop().toJSInteger(theEnv);
        right.eval(theEnv);

        if (op == "&")
            lV.and(theEnv);
        else
        if (op == "|")
            lV.or(theEnv);
        else
        if (op == "^")
            lV.xor(theEnv);
        else
        if (op == "<<")
            lV.shr(theEnv);
        else
        if (op == ">>")
            lV.shl(theEnv);
        else
        if (op == ">>>")
            lV.ushl(theEnv);
        else
            System.out.println("missing bitwise op " + op);
    }

}
