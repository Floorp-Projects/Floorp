

class Brenda {

    static FunctionNode tf;

    public static void main(String cmdArgs[])
    {
        IRFactory irf = new IRFactory(null);        // only need a tokenstream for errors

        Object scriptBlock = irf.createLeaf(TokenStream.BLOCK);                   // will hold the global level stuff
/*

    this constructs a function 'TestFunction' containing 'foo = 0; throw a; return foo;'

*/
        Object funcBlock = irf.createLeaf(TokenStream.BLOCK);
        Object nameFoo1 = irf.createName("foo");
        Object zero = irf.createNumber(new Double(0));
        Object asgn1 = irf.createAssignment(TokenStream.NOP, (Node)nameFoo1, (Node)zero, null, false);
        Object exprStmt = irf.createExprStatement(asgn1, 0);
        irf.addChildToBack(funcBlock, exprStmt);

        Object nameA2 = irf.createName("a");
        Object throwStmt = irf.createThrow(nameA2, 0);
        irf.addChildToBack(funcBlock, throwStmt);

        Object nameFoo2 = irf.createName("foo");
        Object rtrnStmt = irf.createReturn(nameFoo2, 0);
        irf.addChildToBack(funcBlock, rtrnStmt);

        Object args = irf.createLeaf(TokenStream.LP);
        Object function = irf.createFunction("TestFunction", args, funcBlock, "SourceName", 0, 0, null);
        tf = (FunctionNode)(((Node)function).getProp(Node.FUNCTION_PROP));
        irf.addChildToBack(scriptBlock, function);

/*
        a = 1.0
*/
        Object nameA = irf.createName("a");
        Object number1 = irf.createNumber(new Double(1));
        Object setNameA = irf.createAssignment(TokenStream.NOP, (Node)nameA, (Node)number1, null, false);
        Object exprStmt2 = irf.createExprStatement(setNameA, 0);
        irf.addChildToBack(scriptBlock, exprStmt2);

/*
        try {
            c = TestFunction()
        }
        catch (e) {
            b = 2.0
        }
*/
        Object nameC = irf.createName("c");
        Object nameTF = irf.createName("TestFunction");
        Object funCall = irf.createUnary(TokenStream.CALL, nameTF);
        Object setNameC = irf.createAssignment(TokenStream.NOP, (Node)nameC, (Node)funCall, null, false);
        Object exprStmt3 = irf.createExprStatement(setNameC, 0);
        Object tryBlock = irf.createLeaf(TokenStream.BLOCK);
        irf.addChildToBack(tryBlock, exprStmt3);
               
                       
        Object nameB = irf.createName("b");
        Object number2 = irf.createNumber(new Double(2));
        Object setNameB = irf.createAssignment(TokenStream.NOP, (Node)nameB, (Node)number2, null, false);
        Object exprStmt4 = irf.createExprStatement(setNameB, 0);
        Object catchClause = irf.createCatch("e", null, exprStmt4, 0);        
        Object catchBlock = irf.createLeaf(TokenStream.BLOCK);
        irf.addChildToBack(catchBlock, catchClause);
        
        Object tryStmt = irf.createTryCatchFinally(tryBlock, catchBlock, null, 0);

        irf.addChildToBack(scriptBlock, tryStmt);



        Object script = irf.createScript(scriptBlock, "SourceName", 0, 0, null);


        System.out.println(((Node)script).toStringTree());

        Interpreter interp = new Interpreter();
        interp.executeScript((Node)script);

    }

}