class ArithmeticNode extends BinaryNode {

    ArithmeticNode(String aOp, ExpressionNode aLeft, ExpressionNode aRight)
    {
        super(aOp, aLeft, aRight);
    }

    void eval(Environment theEnv)
    {
        left.eval(theEnv);
        JSValue lV = theEnv.theStack.pop();
        right.eval(theEnv);

        if (op == "+")
            lV.add(theEnv);
        else
        if (op == "-")
            lV.subtract(theEnv);
        else
        if (op == "*")
            lV.multiply(theEnv);
        else
        if (op == "/")
            lV.divide(theEnv);
        else
        if (op == "%")
            lV.remainder(theEnv);
        else
            System.out.println("missing arithmetic op " + op);
    }

}