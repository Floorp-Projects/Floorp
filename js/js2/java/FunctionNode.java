class FunctionNode extends ExpressionNode {

    FunctionNode(JSIdentifier aName, ControlNodeGroup aBody, ExpressionNode parameterList)
    {
        fn = new NativeFunction(aBody.getHead());
        name = aName;
        if (parameterList != null) {
            if (parameterList instanceof BinaryNode)
                buildParameterVector((BinaryNode)parameterList);
            else
                fn.parameters.addElement(parameterList);
        }
    }

    void buildParameterVector(BinaryNode x)
    {
        if (x.left instanceof BinaryNode) {
            buildParameterVector((BinaryNode)(x.left));
            fn.parameters.addElement(x.right);
        }
        else {
            fn.parameters.addElement(x.left);
            fn.parameters.addElement(x.right);
        }
    }

    JSValue eval(Environment theEnv)
    {
        theEnv.scope.putProp(theEnv, name, fn);
        return fn;
    }

    JSString name;
    NativeFunction fn;

}