class ThrowNode extends ControlNode {

    ThrowNode(ExpressionNode e)
    {
        super(e);
    }

    ControlNode eval(Environment theEnv)
    {
        ControlNode n = super.eval(theEnv);
        throw new JSException(theEnv.theStack.pop());
    }

}