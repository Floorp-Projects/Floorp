class FunctionNode extends ExpressionNode {

    FunctionNode(JSIdentifier aName, ControlNodeGroup aBody)
    {
        fn = new NativeFunction(aBody.getHead());
        name = aName;
    }

    JSValue eval(Environment theEnv)
    {
        theEnv.scope.putProp(theEnv, name, fn);
        return fn;
    }

    JSString name;
    NativeFunction fn;

}