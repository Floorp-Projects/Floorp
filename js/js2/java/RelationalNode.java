class RelationalNode extends BinaryNode {

    RelationalNode(String aOp, ExpressionNode aLeft, ExpressionNode aRight)
    {
        super(aOp, aLeft, aRight);
    }

    JSValue eval(Environment theEnv)
    {
        JSValue lV = left.eval(theEnv);
        JSValue rV = right.eval(theEnv);

        if (op == ">")
            return lV.gt(theEnv, rV);
        else
        if (op == ">=")
            return lV.ge(theEnv, rV);
        else
        if (op == "<")
            return lV.lt(theEnv, rV);
        else
        if (op == "<=")
            return lV.le(theEnv, rV);
        else
        if (op == "==")
            return lV.eq(theEnv, rV);
        else
        if (op == "!=")
            return lV.ne(theEnv, rV);
        else {
            System.out.println("missing relational op");
            return null;
        }
    }
}
