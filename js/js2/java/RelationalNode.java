class RelationalNode extends BinaryNode {

    RelationalNode(String aOp, ExpressionNode aLeft, ExpressionNode aRight)
    {
        super(aOp, aLeft, aRight);
    }

    void eval(Environment theEnv)
    {
        left.eval(theEnv);
        JSValue lV = theEnv.theStack.pop();
        right.eval(theEnv);

        if (op == ">")
            lV.gt(theEnv);
        else
        if (op == ">=")
            lV.ge(theEnv);
        else
        if (op == "<")
            lV.lt(theEnv);
        else
        if (op == "<=")
            lV.le(theEnv);
        else
        if (op == "==")
            lV.eq(theEnv);
        else
        if (op == "!=")
            lV.ne(theEnv);
        else
            System.out.println("missing relational op");
    }
}
