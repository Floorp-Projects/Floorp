class BitwiseNode extends BinaryNode {

    BitwiseNode(String aOp, ExpressionNode aLeft, ExpressionNode aRight)
    {
        super(aOp, aLeft, aRight);
    }

    JSValue eval(Environment theEnv)
    {
        JSInteger lV = left.eval(theEnv).toJSInteger(theEnv);
        JSValue rV = right.eval(theEnv);

        if (op == "&")
            return lV.and(theEnv, rV);
        else
        if (op == "|")
            return lV.or(theEnv, rV);
        else
        if (op == "^")
            return lV.xor(theEnv, rV);
        else
        if (op == "<<")
            return lV.shr(theEnv, rV);
        else
        if (op == ">>")
            return lV.shl(theEnv, rV);
        else
        if (op == ">>>")
            return lV.ushl(theEnv, rV);
        else {
            System.out.println("missing bitwise op " + op);
            return null;
        }
    }

}
