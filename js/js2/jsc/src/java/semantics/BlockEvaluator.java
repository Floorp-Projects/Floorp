/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mountain View Compiler
 * Company.  Portions created by Mountain View Compiler Company are
 * Copyright (C) 1998-2000 Mountain View Compiler Company. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Jeff Dyer <jeff@compilercompany.com>
 */

package com.compilercompany.ecmascript;
import java.io.*;
import java.lang.reflect.*;
import java.util.*;

/**
 * Delineates the program into its basic execution blocks.
 *
 * It is necessary to know all the blocks in the program 
 * before we can mark the flow-control edges and compute 
 * the dominance relationship between a reference and 
 * definition.
 *
 * The algorithm for partitioning the program into basic
 * blocks goes like this:
 * 1 Identify leaders.
 * 1.1 The first statement of a program.
 * 1.2 The target of a branch.
 * 1.3 Immediately follows a branch.
 * 1.2 Create a block with entry = leader, exit = 
 * 2.Identify exits.
 * 3 Enter a new block at each leader.
 * 4 Exit the current block when an exit is encountered.
 */

public class BlockEvaluator extends Evaluator {

    private static final boolean debug = false;

    /**
     * ExpressionStatementNode
     *
     * Semantics:
     * 1. Tag node with current block label.
     * 2. Return.
     */

    Value evaluate( Context context, ExpressionStatementNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("defining ExpressionStatementNode = " + node);
        }

        node.block = context.getBlock();
        return UndefinedValue.undefinedValue;
    }

    /**
     * EmptyStatementNode
     *
     * Semantics:
     * 1. Tag node with current block label.
     * 2. Return undefined.
     */

    Value evaluate( Context context, EmptyStatementNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("defining ExpressionStatementNode = " + node);
        }

        node.block = context.getBlock();
        return UndefinedValue.undefinedValue;
    }

    /**
     * AnnotatedBlockNode
     *
     * Syntax:
     * { attributes:list statements:list }
     * 
     * Semantics:
     * 1. Tag node with current block label.
     * 2. Exit current block.
     * 3. Mark first statement of block, a leader.
     * 4. Evaluate statements.
     * 5. Return.
     *
     * Notes: 
     * The attributes reside in the preceding block. The first statement is
     * the leader of a new block since a boolean typed attribute affects
     * whether this statement list gets included in the control-flow of the
     * program. The next pass will determine if there is a false valued
     * attribute and exclude this block from the edges that lead from the
     * preceding block.
     */

    Value evaluate( Context context, AnnotatedBlockNode node ) throws Exception { 

        if( debug ) {
            Debugger.trace("evaluating AnnotatedBlockNode = " + node);
        }

        node.block = context.getBlock();
        context.exitBlock(node);
        node.statements.first().markLeader();
        node.statements.evaluate(context,this);
        return UndefinedValue.undefinedValue;
    }

    /**
     * StatementListNode
     */

    Value evaluate( Context context, StatementListNode node ) throws Exception { 

        if( debug ) {
            Debugger.trace("defining StatementListNode = " + node);
        }

        ListValue list;

        if( node.list != null ) {
            if( node.list instanceof StatementListNode ) {
                list = (ListValue) node.list.evaluate(context,this);
            } else {
                list = new ListValue();
                list.push(node.list.evaluate(context,this));
            }
        } else {
            list = new ListValue();
        }

        if( node.item != null ) {

            Node item = node.item;
            StatementListNode prev = (StatementListNode)node.list;

            // Mark leaders.

            if( prev != null && prev.item.isBranch() ) {
                item.markLeader();
            } 

            if( item.isBranch() ) {
                Node[] targets = item.getTargets();
                if( targets!=null ) { 
                    for(int i=0;i<targets.length;i++) {
                        Debugger.trace("targets[i]="+targets[i]);
                        targets[i].markLeader();
                    }
                }
            }

            if( prev != null && item.isLeader() ) {  // This should only happen with case labels.
                context.exitBlock(prev);
            }
             
            if( item.isLeader() ) {
                context.enterBlock(item);
            }
            
            item.evaluate(context,this);

        }
        else {
            context.exitBlock(node.list.item);
        }

        return list;
    }

    static final void testStatementList() throws Exception {

        String[] input = { 
            "x=1;y=2;z=3;",
            "if(x) {x=1;y=2;}",
            "class C {} a=0; if(x) {var x:C;y=2;} else { z=3; } var c:C; c;",
            "a=0; if(x) {x=1;y=2;} else { class C {} z=3; } d=4; var c:C; c;",
        };

        String[] expected = { 
            "",
            "",
            "",
            "",
        };

        testEvaluator("AnnotatedDefinition",input,expected);
    }

    /**
     * LabeledStatementNode
     *
     * Semantics:
     * 1 Tag node with current block label.
     * 2 Evaluate statement.
     * 3 Exit the current block.
     * 4 Return.
     */

    Value evaluate( Context context, LabeledStatementNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("defining LabeledStatementNode = " + node);
        }

        // Point the current statement to its block.
        // This reference will be used to compute dominators.

        node.block = context.getBlock();
        node.statement.evaluate(context,this);
        context.exitBlock(node.statement.last());
        return UndefinedValue.undefinedValue;
    }

    /**
     * IfStatementNode
     *
     * Semantics:
     * 1. Tag node with current block label.
     * 2. Exit current block.
     * 3. Mark as leader the first statement of then block.
     * 4. Evaluate the then block statements.
     * 5. Exit the current block.
     * 6. Mark as leader the first statement of else block.
     * 7. Evaluate the else block statements.
     * 8. Exit the current block.
     * 9. Return.
     */

    Value evaluate( Context context, IfStatementNode node ) throws Exception {
     
        if( debug ) {
            Debugger.trace("evaluating Node = " + node);
        }

        node.block = context.getBlock();
        context.exitBlock(node.condition);
        node.thenactions.markLeader();
        context.enterBlock(node.thenactions);
        node.thenactions.block = context.getBlock();
        node.thenactions.evaluate(context,this);
        context.exitBlock(node.thenactions.last());
        if( node.elseactions!=null ) {
            node.elseactions.markLeader();
            context.enterBlock(node.elseactions);
            node.elseactions.block = context.getBlock();
            node.elseactions.evaluate(context,this);
            context.exitBlock(node.elseactions.last());
        }

        return UndefinedValue.undefinedValue;
    }

    /**
     * SwitchStatementNode
     *
     * { expr, statements }
     *
     * Semantics:
     * 1. Tag node with current block label.
     * 2. Exit current block.
     * 3. Mark as leader the first statement of statement list.
     * 4. Evaluate the statement list.
     * 5. Exit the current block.
     * 9. Return.
     */

    Value evaluate( Context context, SwitchStatementNode node ) throws Exception {
     
        if( debug ) {
            Debugger.trace("evaluating Node = " + node);
        }

        node.block = context.getBlock();
        context.exitBlock(node.expr);
        node.statements.evaluate(context,this);

        return UndefinedValue.undefinedValue;
    }

    /**
     * CaseLabelNode
     * { label }
     * if label is null, then its the default case.
     *
     * Semantics:
     * 1 Tag node with current block label.
     * 4 Return.
     */

    Value evaluate( Context context, CaseLabelNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("defining CaseLabelNode = " + node);
        }

        node.block = context.getBlock();
        return UndefinedValue.undefinedValue;
    }

    /**
     * DoStatementNode
     *
     * { statements, expr }
     *
     * Semantics:
     * 1 Tag node with current block label.
     * 2 Evaluate statements.
     * 3 Exit the current block.
     * 4 Return.
     */

    Value evaluate( Context context, DoStatementNode node ) throws Exception {
     
        if( debug ) {
            Debugger.trace("evaluating DoStatementNode = " + node);
        }

        node.block = context.getBlock();
        node.statements.evaluate(context,this);
        context.exitBlock(node.expr);
        return UndefinedValue.undefinedValue;
    }

    /**
     * WhileStatementNode
     *
     * { expr, statement }
     *
     * Semantics:
     * 1 Tag node with current block label.
     * 2 Evaluate statements.
     * 3 Exit the current block.
     * 4 Return.
     */

    Value evaluate( Context context, WhileStatementNode node ) throws Exception {
     
        if( debug ) {
            Debugger.trace("evaluating WhileStatementNode = " + node);
        }

        node.block = context.getBlock();
        context.exitBlock(node);
        context.enterBlock(node.statement);
        node.statement.evaluate(context,this);
        context.exitBlock(node.statement.last());
        return UndefinedValue.undefinedValue;
    }

    /**
     * ForStatementNode
     *
     * { initialize, test, increment, statement }
     *
     * Semantics:
     * 1 Tag node with current block label.
     * 2 Evaluate statements.
     * 3 Exit the current block.
     * 4 Return.
     */

    Value evaluate( Context context, ForStatementNode node ) throws Exception {
     
        if( true || debug ) {
            Debugger.trace("evaluating ForStatementNode = " + node);
        }

        node.block = context.getBlock();
        context.exitBlock(node);
        context.enterBlock(node.statement);
        node.statement.evaluate(context,this);
        context.exitBlock(node.statement.last());
        return UndefinedValue.undefinedValue;
    }

    /**
     * ForInStatementNode
     *
     * { property, object, statement }
     *
     * Semantics:
     * 1 Tag node with current block label.
     * 2 Evaluate statements.
     * 3 Exit the current block.
     * 4 Return.
     */

    Value evaluate( Context context, ForInStatementNode node ) throws Exception {
     
        if( true || debug ) {
            Debugger.trace("evaluating ForInStatementNode = " + node);
        }

        node.block = context.getBlock();
        context.exitBlock(node);
        context.enterBlock(node.statement);
        node.statement.evaluate(context,this);
        context.exitBlock(node.statement.last());
        return UndefinedValue.undefinedValue;
    }

    /**
     * WithStatementNode
     *
     * Syntax:
     * { expr, statement }
     * 
     * Semantics:
     * 1. Tag node with current block label.
     * 4. Evaluate statement.
     * 5. Return.
     */

    Value evaluate( Context context, WithStatementNode node ) throws Exception { 

        if( debug ) {
            Debugger.trace("evaluating WithStatementNode = " + node);
        }

        node.block = context.getBlock();
        node.statement.evaluate(context,this);
        return UndefinedValue.undefinedValue;
    }

    /**
     * BreakStatementNode
     *
     * Semantics:
     * 1. Tag node with current block label.
     * 2. Exit the current block.
     * 3. Return.
     */

    Value evaluate( Context context, BreakStatementNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("defining BreakStatementNode = " + node);
        }

        // Point the current statement to its block.
        // This reference will be used to compute dominators.

        node.block = context.getBlock();
        context.exitBlock(node);
        return UndefinedValue.undefinedValue;
    }

    /**
     * ContinueStatementNode
     *
     * Semantics:
     * 1. Tag node with current block label.
     * 2. Exit the current block.
     * 3. Return.
     */

    Value evaluate( Context context, ContinueStatementNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("defining ContinueStatementNode = " + node);
        }

        // Point the current statement to its block.
        // This reference will be used to compute dominators.

        node.block = context.getBlock();
        context.exitBlock(node);
        return UndefinedValue.undefinedValue;
    }

    /**
     * ReturnStatementNode
     *
     * Semantics:
     * 1. Tag node with current block label.
     * 2. Exit the current block.
     * 3. Return.
     */

    Value evaluate( Context context, ReturnStatementNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("defining ReturnStatementNode = " + node);
        }

        node.block = context.getBlock();
        context.exitBlock(node);
        return UndefinedValue.undefinedValue;
    }

    /**
     * ThrowStatementNode
     *
     * Semantics:
     * 1. Tag node with current block label.
     * 2. Exit the current block.
     * 3. Return.
     */

    Value evaluate( Context context, ThrowStatementNode node ) throws Exception { 
        if( debug ) {
            Debugger.trace("defining ThrowStatementNode = " + node);
        }

        node.block = context.getBlock();
        context.exitBlock(node);
        return UndefinedValue.undefinedValue;
    }

    /**
     * TryStatementNode
     * { tryblock, catchlist, finallyblock }
     *
     * Semantics:
     * 1. Tag node with current block label.
     * 4. Evaluate the tryblock.
     * 5. Exit the current block.
     * 7. Evaluate the catchblock.
     * 8. Exit the current block.
     * 9. Evaluate the finally block.
     * 10. Exit the current block.
     * 11. Return.
     */

    Value evaluate( Context context, TryStatementNode node ) throws Exception {
     
        if( debug ) {
            Debugger.trace("evaluating Node = " + node);
        }

        node.block = context.getBlock();
        node.tryblock.evaluate(context,this);
        context.exitBlock(node.tryblock.last());
        if( node.catchlist!=null ) {
            node.catchlist.evaluate(context,this);
            context.exitBlock(node.catchlist.last());
        }
        if( node.finallyblock!=null ) {
            context.enterBlock(node.finallyblock);
            node.finallyblock.evaluate(context,this);
            context.exitBlock(node.finallyblock.last());
        }

        return UndefinedValue.undefinedValue;
    }

    /**
     * CatchClauseNode
     *
     * Syntax:
     * { parameter statements }
     * 
     * Semantics:
     * 1. Tag node with current block label.
     * 2. Evaluate statements.
     * 3. Return.
     */

    Value evaluate( Context context, CatchClauseNode node ) throws Exception { 

        if( debug ) {
            Debugger.trace("evaluating CatchClauseNode = " + node);
        }

        node.block = context.getBlock();
        node.statements.evaluate(context,this);
        return UndefinedValue.undefinedValue;
    }

    /**
     * FinallyClauseNode
     *
     * Syntax:
     * { statements }
     * 
     * Semantics:
     * 1. Tag node with current block label.
     * 2. Evaluate statements.
     * 3. Return.
     */

    Value evaluate( Context context, FinallyClauseNode node ) throws Exception { 

        if( debug ) {
            Debugger.trace("evaluating FinallyClauseNode = " + node);
        }

        node.block = context.getBlock();
        node.statements.evaluate(context,this);
        return UndefinedValue.undefinedValue;
    }

    // Definitions

    /**
     * namespaces is used to store namespace attributes for the
     * current definition being evaluated.
     */

    private Vector namespaces = new Vector();
    private Vector parameters = new Vector();

    /**
     * AnnotatedDefinition
     */

    public Value evaluate( Context context, AnnotatedDefinitionNode node ) throws Exception {

        if( debug ) {
            Debugger.trace( "defining AnnotatedDefinitionNode: " + node );
        }

        node.block = context.getBlock();
        Scope local = context.getLocal();

        if( node.definition != null ) {
            node.definition.evaluate(context,this);
        }

        return UndefinedValue.undefinedValue;
    }

    static final void testAnnotatedDefinition() throws Exception {

        String[] input = { 
            "",
        };

        String[] expected = { 
            "",
        };

        testEvaluator("AnnotatedDefinition",input,expected);
    }

    /**
     * ExportDefinition
     */

    public Value evaluate( Context context, ExportDefinitionNode node ) throws Exception {

        if( debug ) {
            Debugger.trace( "defining ExportDefinitionNode: " + node );
        }

        node.block = context.getBlock();
        return UndefinedValue.undefinedValue;
    }

    static final void testExportDefinition() throws Exception {

        String[] input = { 
            "",
        };

        String[] expected = { 
            "",
        };

        testEvaluator("ExportDefinition",input,expected);
    }

    /**
     * VariableDefinition
     */

    public Value evaluate( Context context, VariableDefinitionNode node ) throws Exception {

        if( debug ) {
            Debugger.trace( "defining VariableDefinitionNode: " + node );
        }

        node.block = context.getBlock();
        return UndefinedValue.undefinedValue;
    }

    static final void testVariableDefinition() throws Exception {

        String[] input = { 
            "var x; x;",
            "var y:T; y;",
            "class C; class C{}; var x, y:C; y;",
            "var x = 1; const y = 2; var z = n;",
            "var x = 1,y = 2,z = n;",
            "if(true) {const n = 1;} const z = n;",
        };

        String[] expected = { 
            "code(expressionstatement( identifier( x ) ))",
            "error(Type expression does not evaluate to a type: undefined)",
            "code(expressionstatement( identifier( y ) ))",
            "",
            "",
            "",
        };

        testEvaluator("VariableDefinition",input,expected);
    }

    /**
     * FunctionDeclaration
     */

    public Value evaluate( Context context, FunctionDeclarationNode node ) throws Exception {

        if( debug ) {
            Debugger.trace( "defining FunctionDeclarationNode: " + node );
        }

        node.block = context.getBlock();
        return UndefinedValue.undefinedValue;
    }

    static final void testFunctionDeclaration() throws Exception {

        String[] input = { 
            "function f(); f;",
            "function f(x); f;",
            "function f(x:Integer,y:String); f;",
        };

        String[] expected = { 
            "completion( 0, object(function(f)){object(namespace(_parameters_)){}=object(namespace(object(namespace(_parameters_)){})){_result_={ 0, typevalue(any){}, undefined }}}, null )",
            "completion( 0, object(function(f)){object(namespace(_parameters_)){}=object(namespace(object(namespace(_parameters_)){})){x={ 0, typevalue(any){}, undefined }, _result_={ 0, typevalue(any){}, undefined }}}, null )",
            "completion( 0, object(function(f)){object(namespace(_parameters_)){}=object(namespace(object(namespace(_parameters_)){})){x={ 0, typevalue(integer){}, undefined }, _result_={ 0, typevalue(any){}, undefined }, y={ 0, typevalue(string){}, undefined }}}, null )",
        };

        testEvaluator("FunctionDeclaration",input,expected);
    }

    /**
     * FunctionDefinition
     */

    public Value evaluate( Context context, FunctionDefinitionNode node ) throws Exception {

        if( debug ) {
            Debugger.trace( "defining FunctionDefinitionNode: " + node );
        }

        node.block = context.getBlock();
        context.exitBlock(node.decl);
        if( node.body!=null) {
            node.body.first().markLeader();
            node.body.evaluate(context,this);
            context.exitBlock(node.body.last());
        }

        return UndefinedValue.undefinedValue;
    }

    static final void testFunctionDefinition() throws Exception {

        String[] input = { 
            "function f(){}; f",
            "function f(x){var a;}; f",
            "namespace V; const v=namespace(V);v function f(x,y){var a; const b;}; v::f",
        };

        String[] expected = { 
            "completion( 0, object(function(f)){object(namespace(_parameters_)){}=object(namespace(object(namespace(_parameters_)){})){_result_={ 0, typevalue(any){}, undefined }}}, null )",
            "completion( 0, object(function(f)){a={ 0, typevalue(any){}, undefined }, object(namespace(_parameters_)){}=object(namespace(object(namespace(_parameters_)){})){x={ 0, typevalue(any){}, undefined }, _result_={ 0, typevalue(any){}, undefined }}, _code_={ 0, any, list([list([])]) }}, null )",
            "",
        };

        testEvaluator("FunctionDefinition",input,expected);
    }

    /**
     * ClassDeclaration
     */

    public Value evaluate( Context context, ClassDeclarationNode node ) throws Exception {

        if( debug ) {
            Debugger.trace( "defining ClassDeclarationNode: " + node );
        }

        node.block = context.getBlock();
        return UndefinedValue.undefinedValue;
    }

    static final void testClassDeclaration() throws Exception {

        String[] input = { 
            "class C; C;"
        };

        String[] expected = { 
            "completion( 0, typevalue(class(C extends null)){}, null )"
        };

        testEvaluator("ClassDeclaration",input,expected);
    }

    /**
     * ClassDefinition
     *
     * 1. Create a new class object.
     * 2. Add the classname and object to the global type database.
     * 3. Push the class object onto the scope stack.
     */

    public Value evaluate( Context context, ClassDefinitionNode node ) throws Exception {

        if( debug ) {
            Debugger.trace( "defining ClassDefinitionNode: " + node );
        }

        node.block = context.getBlock();

        if( node.statements!=null ) {
            node.statements.evaluate(context,this);
        }

        return UndefinedValue.undefinedValue;
    }

    static final void testClassDefinition() throws Exception {

        String[] input = { 
            "class C {} C",
        };

        String[] expected = { 
            "completion( 0, typevalue(class(C extends null)){}, null )",
        };

        testEvaluator("ClassDefinition",input,expected);
    }

    /**
     * NamespaceDefinition
     */

    public Value evaluate( Context context, NamespaceDefinitionNode node ) throws Exception {

        if( debug ) {
            Debugger.trace( "defining NamespaceDefinitionNode: " + node );
        }

        node.block = context.getBlock();
        return UndefinedValue.undefinedValue;
    }

    static final void testNamespaceDefinition() throws Exception {

        String[] input = { 
            "namespace N; N;",
            "namespace N2 extends N1; N2;",
        };

        String[] expected = { 
            "completion( 0, object(namespace(N)){}, null )",
            "completion( 0, object(namespace(N2)){}, null )",
        };

        testEvaluator("NamespaceDefinition",input,expected);
    }

    /**
     * Program
     */

    public Value evaluate( Context context, ProgramNode node ) throws Exception {

        if( debug ) {
            Debugger.trace( "defining ProgramNode: " + node );
        }

        node.statements.first().markLeader();
        node.statements.evaluate(context,this);
        context.exitBlock(node.statements.last());
        return UndefinedValue.undefinedValue;

    }

    /**
     * Main. Runs self diagnostics.
     */

    static int failureCount = 0;

    public static void main( String[] args ) {
        PrintStream outfile = null;
        try {

            outfile = new PrintStream( new FileOutputStream( "Parser.test" ) );
            System.setOut( outfile );

            Debugger.trace( "Parser test begin" );


            if( failureCount != 0 )
                Debugger.trace( "DefinitionEvaluator test completed: " + failureCount + " tests failed" );
            else
                Debugger.trace( "DefinitionEvaluator test completed successfully" );
            }
            catch( Exception e ) {
                //Debugger.trace( "compile error: " + e );
                e.printStackTrace();
        }
        finally {
            outfile.close();
            System.setOut( null );
        }
    }

    static final void testEvaluator(String name, String[] input, String[] expected) throws Exception {

        Debugger.trace( "begin testEvaluator: " + name );

        String result = "";

        Node node;
        Value type;
        Evaluator evaluator;

        Class pc = Parser.class;
        Class[] args = new Class[0];

        Parser parser;
        long t=0;

        ObjectValue global = null;

        for( int i = 0; i < input.length; i++ ) {

            Context context = new Context();
 
            try {

                global = new ObjectValue("__systemGlobal", new GlobalObject());
                context.enterClass(global);

                parser    = new Parser(new StringReader(input[i]));
                evaluator = new BlockEvaluator();

                System.gc();
	            t = System.currentTimeMillis();
                node = parser.parseProgram();
                Debugger.trace("    I: "+global);
                context.evaluator = evaluator;
                node.evaluate(context, evaluator);
                Debugger.trace("    D: "+global);

	            t = System.currentTimeMillis() - t;
                context.exitClass();

            } catch( Exception x ) {
                x.printStackTrace();
	            t = System.currentTimeMillis() - t;
                result = "error("+x.getMessage()+")";
                context.exitClass();
            }
            if( result.equals( expected[i] ) ) {
                Debugger.trace( "  " + i + " passed in " + Long.toString(t) + 
                                " msec [" + input[i] + "] = " + result );
            } else {
                failureCount++;
                Debugger.trace( "  " + i + " failed [" + input[i] + "] = " + result );
            }

        }

        Debugger.trace( "finish testEvaluator: " + name );
    }


}

/*
 * The end.
 */
