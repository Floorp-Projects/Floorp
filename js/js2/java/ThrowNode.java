class ThrowNode extends ControlNode {

    ThrowNode(ExpressionNode e)
    {
        super(e);
    }

    ControlNode eval(Environment theEnv)
    {
        throw new JSException(expr.eval(theEnv));
    }

}