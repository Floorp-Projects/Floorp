class ArithmeticNode extends BinaryNode {

    ArithmeticNode(String aOp, ExpressionNode aLeft, ExpressionNode aRight)
    {
        super(aOp, aLeft, aRight);
    }

    JSValue eval(Environment theEnv)
    {
        JSValue lV = left.eval(theEnv);
        JSValue rV = right.eval(theEnv);

        if (op == "+")
            return lV.add(theEnv, rV);
        else
        if (op == "-")
            return lV.subtract(theEnv, rV);
        else
        if (op == "*")
            return lV.multiply(theEnv, rV);
        else
        if (op == "/")
            return lV.divide(theEnv, rV);
        else
        if (op == "%")
            return lV.remainder(theEnv, rV);
        else {
            System.out.println("missing arithmetic op " + op);
            return null;
        }
    }

}