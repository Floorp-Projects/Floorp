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
import java.util.Vector;
import java.io.*;
import java.lang.reflect.*;

/**
 * Parse JS2 programs.
 */

public class Parser implements Tokens, Errors {

    private static final boolean debug = true;
    private static final boolean debug_lookahead = false;
    private static final boolean debug_match = false;

    private   Scanner scanner;
    protected int     lasttoken   = empty_token;
    protected int     nexttoken   = empty_token;
    protected int     thistoken   = empty_token;
    private   boolean isNewLine   = false;
    protected Vector  statements  = null;
    protected Vector  functions   = null;
	private   int     errors      = 0;

    public static PrintStream out;
    public static void setOut(String filename) throws Exception {
        out = new PrintStream( new FileOutputStream(filename) );
    }

    /**
     * self tests.
     */

    static int failureCount = 0;

    public static void main( String[] args ) {
        try {

            Debugger.setDbgFile("debug.dbg");
            Debugger.setOutFile("debug.out");
            Debugger.setErrFile("debug.err");

            Debugger.trace( "Parser test begin" );
            
            // Expressions

            testIdentifier();
            testSimpleQualifiedIdentifier();
            testQualifiedIdentifier();
            testPrimaryExpression();
            testObjectLiteral();
            testArrayLiteral();
            testAttributeExpression();
            testPostfixExpression();
            testConstExpression();
            testConstDotExpression();
            testBracketExpression();
            testBracketSubexpression();
            testMultiplicativeExpression();
            testAdditiveExpression();
            testArgumentList();
            testAssignmentExpression();
            testListExpression();
            testTypeExpression();

            // Statements

            testTopStatement();
            testStatement();
            testIfStatement();
            testExpressionStatement();
            testBlock();
            testTopStatements();
            testUseStatement();
            testReturnStatement();

            // Definitions

            testAnnotatedDefinition();
            testAttributes();
            testAttribute();
            testDefinition();
            testVariableDefinition();
            testVariableBindingList();
            testTypedVariable();
            testFunctionDefinition();
            testFunctionSignature();
            testParameters();
            testParameter();
            testNamedParameters();
            testRestParameter();
            testResultSignature();
            testClassDefinition();
            testProgram();

            if( failureCount != 0 ) {
                Debugger.trace( "Parser test completed: " + failureCount + " tests failed" );
            } else {
                Debugger.trace( "Parser test completed successfully" );
            }
        } catch( Exception e ) {
            Debugger.trace( "compile error: " + e );
            e.printStackTrace();
        } finally {
            System.setOut( null );
        }
    }

    static final void testParser(String name, String[] input, String[] expected) throws Exception {

        Debugger.trace( "begin testParser: " + name );

        String result;

        Class pc = Parser.class;
        Class[] args = new Class[0];
        Method m = pc.getMethod("parse"+name,args);

        Parser parser;
        long t=0;

        for( int i = 0; i < input.length; i++ ) {
            try {
                parser = new Parser(new StringReader(input[i]));
                System.gc();
	            t = System.currentTimeMillis();
                result = "" + m.invoke(parser,args);
                t = System.currentTimeMillis() - t;
            } catch( InvocationTargetException x ) {
	            t = System.currentTimeMillis() - t;
                result = "error: " + x.getTargetException().getMessage();
                if( debug ) {
                    x.printStackTrace();
                }
            }
            if( result.equals( expected[i] ) ) {
                Debugger.trace( "  " + i + " passed [" + Long.toString(t) + 
                                " msec]: " + input[i] /*+ " --> " + result*/);
            } else {
                failureCount++;
                Debugger.trace( "  " + i + " failed: " + input[i] + " --> " + result );
            }
        }

        Debugger.trace( "finish testParser: " + name );
    }

    /**
     * 
     */

    public Parser (Reader reader) {
        this.scanner = new Scanner (reader,System.err,"test");
        this.statements = new Vector();
        this.functions  = new Vector();
        NodeFactory.init(scanner.input);
        Context.init(scanner.input);
    }

    /**
     *
     */

    public boolean newline() throws Exception {
        if( nexttoken == empty_token ) {
            nexttoken = scanner.nexttoken();
        }
        return scanner.followsLineTerminator();
    }

    private static int abbrevIfElse_mode = else_token;   // lookahead uses this value. don't change.
    private static int abbrevDoWhile_mode = while_token; // ditto.
    private static int abbrev_mode = last_token-1;
    private static int full_mode = abbrev_mode-1;

    private static int allowIn_mode = full_mode-1;
    private static int noIn_mode = allowIn_mode-1;


    /**
     * Match the current input with an expected token. lookahead is managed by 
     * setting the state of this.nexttoken to empty_token after an match is
     * attempted. the next lookahead will initialize it again.
     */

    final public int match( int expectedTokenClass ) throws Exception {

	    if( debug ) {
	        Debugger.trace( "matching " + Token.getTokenClassName(expectedTokenClass) + " with input " +
	                      Token.getTokenClassName(scanner.getTokenClass(nexttoken)) );
        }

        int result = error_token;

        if( !lookahead( expectedTokenClass ) ) {
            scanner.error(scanner.syntax_error,"Expecting " + Token.getTokenClassName(expectedTokenClass) +
	                      " before " + scanner.getTokenSource(nexttoken),error_token);
            nexttoken = empty_token;
			throw new Exception("syntax error");
        } else
        if( expectedTokenClass != scanner.getTokenClass( nexttoken ) ) {
	        result    = thistoken;
	    } else {
	        result    = nexttoken;
            lasttoken = nexttoken;
            nexttoken = empty_token;
	    }

	    if( (debug || debug_match) ) {
	        Debugger.trace( "match " + Token.getTokenClassName(expectedTokenClass) );
        }

        return result;
    }

    /**
     * Match the current input with an expected token. lookahead is managed by 
     * setting the state of this.nexttoken to empty_token after an match is
     * attempted. the next lookahead will initialize it again.
     */

    final public boolean lookaheadSemicolon(int mode) throws Exception {

	    if( debug ) {
	        Debugger.trace( "looking for virtual semicolon with input " +
	                      Token.getTokenClassName(scanner.getTokenClass(nexttoken)) );
        }

        boolean result = false;

        if( lookahead(semicolon_token) ||
            lookahead(eos_token) ||
            lookahead(rightbrace_token) ||
            lookahead(mode) ||
            scanner.followsLineTerminator() ) {
	        result = true;
	    } 

        return result;
    }

    final public boolean lookaheadNoninsertableSemicolon(int mode) throws Exception {

	    if( debug ) {
	        Debugger.trace( "looking for virtual semicolon with input " +
	                      Token.getTokenClassName(scanner.getTokenClass(nexttoken)) );
        }

        boolean result = false;

        if( lookahead(eos_token) ||
            lookahead(rightbrace_token) ||
            lookahead(mode) ) {
	        result = true;
	    } 

        return result;
    }

    final public int matchSemicolon(int mode) throws Exception {

	    if( debug ) {
	        Debugger.trace( "matching semicolon with input " +
	                      Token.getTokenClassName(scanner.getTokenClass(nexttoken)) );
        }

        int result = error_token;

        if( lookahead( semicolon_token ) ) {
            result = match(semicolon_token);
        } else if( scanner.followsLineTerminator() || 
            lookahead(eos_token) ||
            lookahead(rightbrace_token) ) {
	        result = semicolon_token;
        } else if( (mode == abbrevIfElse_mode || mode == abbrevDoWhile_mode) 
                   && lookahead(mode) ) {
	        result = semicolon_token;
	    } else {
            throw new Exception( "Expecting semicolon before '" + scanner.getTokenSource(nexttoken) +
                          "' in the text '" + scanner.getLineText() + "'" );
	    }

        return result;
    }

    /**
     * Match a non-insertable semi-colon. This function looks for
     * a semicolon_token or other grammatical markers that indicate
     * that the empty_token is equivalent to a semicolon.
     */

    final public int matchNoninsertableSemicolon(int mode) throws Exception {

	    if( debug ) {
	        Debugger.trace( "matching semicolon with input " +
	                      Token.getTokenClassName(scanner.getTokenClass(nexttoken)) );
        }

        int result = error_token;

        if( lookahead( semicolon_token ) ) {
            result = match(semicolon_token);
        } else if( lookahead(eos_token) ||
                   lookahead(rightbrace_token) ) {
	        result = semicolon_token;
        } else if( (mode == abbrevIfElse_mode || mode == abbrevDoWhile_mode) 
                   && lookahead(mode) ) {
	        result = semicolon_token;
	    } else {
            throw new Exception( "Expecting semicolon before '" + scanner.getTokenSource(nexttoken) +
                          "' in the text '" + scanner.getLineText() + "'" );
	    }

        return result;
    }

    /**
     * Lookahead to the next token.
     */

    final public boolean lookahead( int expectedTokenClass ) throws Exception {

        boolean result = false;

	    // If the nexttoken is empty_token, then fetch another. This is the first
	    // lookahead since the last match.
	    
        if( nexttoken == empty_token ) {
            nexttoken = scanner.nexttoken();
            if(Scanner.out != null) {
                Scanner.out.println((scanner.followsLineTerminator()?"*":"")+Token.getTokenClassName(scanner.getTokenClass(nexttoken)));
            }
        }

	    if( (debug || debug_lookahead) ) {
            //Debugger.trace( "looking for " + Token.getTokenClassName(expectedTokenClass) );
	        Debugger.trace( "lookahead " + Token.getTokenClassName(scanner.getTokenClass(nexttoken)) );
        }

	    // Check for invalid token.
    
	    if( nexttoken == error_token ) {
	        throw new Exception( "Invalid word " + scanner.lexeme() +
						  " in text " + scanner.getLineText() + "\n" );
        }

	    // Compare the expected token class against the token class of 
        // the nexttoken.

        if( expectedTokenClass != scanner.getTokenClass( nexttoken ) ) {
            return false;
        } else {
            thistoken = expectedTokenClass;
            return true;
	    }
    }

	public int errorCount() {
	    return scanner.errorCount;
	}

    /**
     * Start grammar.
     */

    /**
     * Identifier	
     *     Identifier
     *     get
     *     set
     */

    final public IdentifierNode parseIdentifier() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseIdentifier" );
        }

        IdentifierNode $$ = null;

        if( lookahead( get_token ) ) {
            match( get_token );
            $$ = NodeFactory.Identifier("get");
        } else if( lookahead( set_token ) ) {
            match( set_token );
            $$ = NodeFactory.Identifier("set");
        } else if( lookahead( identifier_token ) ) {
            $$ = NodeFactory.Identifier(scanner.getTokenText(match(identifier_token)));
        } else {
            scanner.error(scanner.syntax_error,"Expecting an Identifier.");
			throw new Exception();
        }

        if( debug ) {
            Debugger.trace( "finish parseIdentifier" );
        }
        return $$;
    }

    static final void testIdentifier() throws Exception {

        Debugger.trace( "begin testIdentifier" );

        String[] input = { 
            "xx",
            "get",
            "set", 
        };

        String[] expected = { 
            "identifier( xx )", 
            "identifier( get )", 
            "identifier( set )",
        };

        testParser("Identifier",input,expected);
    }

    /**
     * Qualifier	
	 *     public
	 *     private
	 *     super
	 *     Identifier
     */

    final public Node parseQualifier() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseQualifier" );
        }

        Node $$;

        if( lookahead( public_token ) ) {
            match( public_token );
            $$ = NodeFactory.Identifier("public");
        } else if( lookahead( private_token ) ) {
            match( private_token );
            $$ = NodeFactory.Identifier("private");
        } else if( lookahead( super_token ) ) {
            match( super_token );
            $$ = NodeFactory.Identifier("super");
        } else {
            $$ = parseIdentifier();
        }

        if( debug ) {
            Debugger.trace( "finish parseQualifier" );
        }

        return $$;
    }

    /**
     * SimpleQualifiedIdentifier	
     *     Identifier
     *     Qualifier :: Identifier
     */
	
    final public Node parseSimpleQualifiedIdentifier() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseSimpleQualifiedIdentifier" );
        }

        Node $$,$1;

        if( lookahead( public_token ) ) {
            match(public_token);
            $1 = NodeFactory.Identifier("public");
            match(doublecolon_token);
            $$ = NodeFactory.QualifiedIdentifier($1,parseIdentifier());
        } else if( lookahead(private_token) ) {
            match(private_token);
            $1 = NodeFactory.Identifier("private");
            match(doublecolon_token);
            $$ = NodeFactory.QualifiedIdentifier($1,parseIdentifier());
        } else if( lookahead(super_token) ) {
            match(super_token);
            $1 = NodeFactory.Identifier("super");
            match(doublecolon_token);
            $$ = NodeFactory.QualifiedIdentifier($1,parseIdentifier());
        } else {
            $1 = parseIdentifier();
            if( lookahead(doublecolon_token) ) {
                match(doublecolon_token);
                $$ = NodeFactory.QualifiedIdentifier($1,parseIdentifier());
            } else {
                $$ = $1;
            }
        }
                 
        if( debug ) {
            Debugger.trace( "finish parseSimpleQualifiedIdentifier" );
        }

        return $$;
    }

    static final void testSimpleQualifiedIdentifier() throws Exception {

        Debugger.trace( "begin testSimpleQualifiedIdentifier" );

        String[] input = { 
            "x",
            "get",
            "set", 
	        "public::x",
	        "private::x",
	        "super::x",
            "x::y::z",
        };

        String[] expected = { 
            "qualifiedidentifier( null, x )", 
            "qualifiedidentifier( null, get )", 
            "qualifiedidentifier( null, set )", 
            "qualifiedidentifier( list( null, identifier( public ) ), x )",
            "qualifiedidentifier( list( null, identifier( private ) ), x )",
            "qualifiedidentifier( list( null, identifier( super ) ), x )",
            "qualifiedidentifier( list( list( null, identifier( x ) ), identifier( y ) ), z )",
        };

        testParser("SimpleQualifiedIdentifier",input,expected);
    }

    /**
     * ExpressionQualifiedIdentifier	
     *     ParenthesizedExpression :: Identifier
     */

    final public Node parseExpressionQualifiedIdentifier() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseExpressionQualifiedIdentifier" );
        }

        Node $$,$1;

        $1 = parseParenthesizedExpression();
        match(doublecolon_token);
        $$ = NodeFactory.QualifiedIdentifier($1,parseIdentifier());

        if( debug ) {
            Debugger.trace( "finish parseExpressionQualifiedIdentifier" );
        }

        return $$;
    }

    /**
     * QualifiedIdentifier	
     *     SimpleQualifiedIdentifier
     *     ExpressionQualifiedIdentifier
     */
	
    final public Node parseQualifiedIdentifier() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseQualifiedIdentifier" );
        }

        Node $$,$1;

        if( lookahead( leftparen_token ) ) {

            $$ = parseExpressionQualifiedIdentifier();

        } else {

            $$ = parseSimpleQualifiedIdentifier();

        }
         
        if( debug ) {
            Debugger.trace( "finish parseQualifiedIdentifier" );
        }

        return $$;
    }

    static final void testQualifiedIdentifier() throws Exception {

        Debugger.trace( "begin testQualifiedIdentifier" );

        String[] input = { 
            "xx",
            "get",
            "set", 
	        "public::x",    // public :: QualifiedIdentifier
	        "private::x",   // private :: QualifieldIdentifier
	        "super::x",     // super :: QualifiedItentifier
	        "(y)::x",       // ParenthesizedExpression :: QualifiedIdentifier
	        "z::y::x",       // Identifier QualifiedIdentifierPrime
        };

        String[] expected = { 
            "qualifiedidentifier( null, xx )", 
            "qualifiedidentifier( null, get )", 
            "qualifiedidentifier( null, set )",
            "qualifiedidentifier( list( null, identifier( public ) ), x )",
            "qualifiedidentifier( list( null, identifier( private ) ), x )",
            "qualifiedidentifier( list( null, identifier( super ) ), x )",
            "qualifiedidentifier( qualifiedidentifier( null, y ), x )",
            "qualifiedidentifier( list( list( null, identifier( z ) ), identifier( y ) ), x )",
        };

        testParser("QualifiedIdentifier",input,expected);
    }

    /**
     * PrimaryExpression	
     *     null
     *     true
     *     false
     *     public
     *     Number UnitOperator
     *     String
     *     this
     *     super
     *     RegularExpression
     *     ParenthesizedListExpression UnitOperator
     *     ArrayLiteral
     *     ObjectLiteral
     *     FunctionExpression
     */

    final public Node parsePrimaryExpression() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parsePrimaryExpression" );
        }

        Node $$;

        if( lookahead( null_token ) ) {
            match( null_token );
            $$ = NodeFactory.LiteralNull();
        } else if( lookahead( true_token ) ) {
            match( true_token );
            $$ = NodeFactory.LiteralBoolean(true);
        } else if( lookahead( false_token ) ) {
            match( false_token );
            $$ = NodeFactory.LiteralBoolean(false);
        } else if( lookahead( public_token ) ) {
            match( public_token );
            $$ = NodeFactory.Identifier("public");
        } else if( lookahead( numberliteral_token ) ) {
            Node $1 = NodeFactory.LiteralNumber(scanner.getTokenText(match(numberliteral_token)));
            $$ = parseUnitOperator($1);
        } else if( lookahead(stringliteral_token) ) {
            $$ = NodeFactory.LiteralString(scanner.getTokenText(match(stringliteral_token)));
        } else if( lookahead(this_token) ) {
            match( this_token );
            $$ = NodeFactory.ThisExpression();
        } else if( lookahead(super_token) ) {
            match( super_token );
            $$ = NodeFactory.SuperExpression();
        } else if( lookahead(regexpliteral_token) ) {
            $$ = NodeFactory.LiteralRegExp(scanner.getTokenText(match(regexpliteral_token)));
        } else if( lookahead(leftparen_token) ) {
            Node $1 = parseParenthesizedListExpression();
            $$ = parseUnitOperator($1);
        } else if( lookahead(leftbracket_token) ) {
            $$ = parseArrayLiteral();
        } else if( lookahead(leftbrace_token) ) {
            $$ = parseObjectLiteral();
        } else if( lookahead(function_token) ) {
            $$ = parseFunctionExpression();
        } else {
            scanner.error(scanner.syntax_error,"Expecting <primary expression> before '" + 
                          scanner.getTokenSource(nexttoken),error_token);
			throw new Exception();
        }

        if( debug ) {
            Debugger.trace( "finish parsePrimaryExpression" );
        }

        return $$;
    }

    static final void testPrimaryExpression() throws Exception {

        Debugger.trace( "begin testPrimaryExpression" );

        String[] input = { 
            "null",               //null
            "true",               //true
            "false",              //false
            "public",             //public
            "123",                //Number
            "123 'miles'",        //Number [no line break] String
            "'abc'",              //String
            "this",               //this
            "super",              //super
	        "/xyz/gim",           //RegularExpression
	        "(y)",                //ParenthesizedExpression
	        "(y) 'pesos'",        //ParenthesizedExpression [no line break] String
	        "[a,b,c]",            //ArrayLiteral
	        "{a:1,b:2,c:3}",      //LiteralObject
	        "function f(x,y) {}", //FunctionExpression
        };

        String[] expected = {
            "literalnull", 
            "literalboolean( true )", 
            "literalboolean( false )", 
            "identifier( public )", 
            "literalnumber( 123 )", 
            "unitexpression( literalnumber( 123 ), literalstring( miles ) )", 
            "literalstring( abc )", 
            "thisexpression", 
            "superexpression", 
            "literalregexp( /xyz/gim )", 
            "parenthesizedexpression( qualifiedidentifier( null, y ) )", 
            "unitexpression( parenthesizedexpression( qualifiedidentifier( null, y ) ), literalstring( pesos ) )", 
            "literalarray( list( list( list( null, qualifiedidentifier( null, a ) ), qualifiedidentifier( null, b ) ), qualifiedidentifier( null, c ) ) )",
            "literalobject( list( list( list( null, literalfield( identifier( a ), literalnumber( 1 ) ) ), literalfield( identifier( b ), literalnumber( 2 ) ) ), literalfield( identifier( c ), literalnumber( 3 ) ) ) )",
            "functionexpression( identifier( f ), functionsignature( list( parameter( identifier( x ), null ), parameter( identifier( y ), null ) ), null ), null )",
        };

        testParser("PrimaryExpression",input,expected);
    }

    /**
     * ParenthesizedExpression	
     *     ( AssignmentExpressionallowIn )
     */

    final public Node parseParenthesizedExpression() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseParenthesizedExpression" );
        }

        Node $$;
    
        match( leftparen_token ); 
		int mark = scanner.input.positionOfMark();

        $$ = NodeFactory.ParenthesizedExpression(parseAssignmentExpression(allowIn_mode));

        $$.position = mark;

        match( rightparen_token );
         
        if( debug ) {
            Debugger.trace( "finish parseParenthesizedExpression" );
        }

        return $$;
    }

    static final void testParenthesizedExpression() throws Exception {

        String[] input = { 
	        "",
	        "" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("ParenthesizedExpression",input,expected);
    }

    /**
     * ParenthesizedListExpression	
     *     ParenthesizedExpression
     *     ( ListExpressionallowIn , AssignmentExpressionallowIn )
     */

    final public ParenthesizedListExpressionNode parseParenthesizedListExpression() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseParenthesizedListExpression" );
        }

        ParenthesizedListExpressionNode $$;
    
        match( leftparen_token ); 

        $$ = NodeFactory.ParenthesizedListExpression(parseListExpression(allowIn_mode));

        match( rightparen_token );
         
        if( debug ) {
            Debugger.trace( "finish parseParenthesizedListExpression" );
        }

        return $$;
    }

    static final void testParenthesizedListExpression() throws Exception {

        String[] input = { 
	        "",
	        "" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("ParenthesizedListExpression",input,expected);
    }

    /**
     * UnitOperator	
     *     «empty»
     *     [no line break] String
     */

    final public Node parseUnitOperator(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseUnitOperator" );
        }

        Node $$;
    
        if( lookahead(stringliteral_token) ) {
            Node $2 = NodeFactory.LiteralString(scanner.getTokenText(match(stringliteral_token)));
            $$ = NodeFactory.UnitExpression($1,$2);
        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseUnitOperator" );
        }

        return $$;
    }

    /**
     * PrimaryExpressionOrExpressionQualifiedIdentifier	
     *     ParenthesizedListExpression {if $1.isParenExpr() && lookahead(doublecolon_token)} ...
     *     ParentehsizedListExpression UnitOperator
     *     PrimaryExpression
     */

    final public Node parsePrimaryExpressionOrExpressionQualifiedIdentifier() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parsePrimaryExpressionOrExpressionQualifiedIdentifier" );
        }

        Node $$;
    
        if( lookahead(leftparen_token) ) {
            ParenthesizedListExpressionNode $1 = parseParenthesizedListExpression();
            if( !$1.isListExpression() && lookahead(doublecolon_token) ) {
                match(doublecolon_token);
                $$ = NodeFactory.QualifiedIdentifier($1,parseIdentifier());
            } else {
                $$ = parseUnitOperator($1);
            }
        } else {
            $$ = parsePrimaryExpression();
        }

        if( debug ) {
            Debugger.trace( "finish parsePrimaryExpressionOrExpressionQualifiedIdentifier" );
        }

        return $$;
    }

    /**
     * FunctionExpression	
     *     function FunctionSignature Block
     *     function Identifier FunctionSignature Block
     */

    final public Node parseFunctionExpression() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseFunctionExpression" );
        }

        Node $$,$1,$2,$3;
    
        match(function_token); 

        if( lookahead(leftparen_token) ) {
            $1 = null;
        } else {
            $1 = parseIdentifier();
        }

        $2 = parseFunctionSignature();
        $3 = parseBlock();
         
        $$ = NodeFactory.FunctionExpression($1,$2,$3);

        if( debug ) {
            Debugger.trace( "finish parseFunctionExpression" );
        }

        return $$;
    }

    static final void testFunctionExpression() throws Exception {

        String[] input = { 
	        "",
	        "" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("FunctionExpression",input,expected);
    }

    /**
     * ObjectLiteral	
     *     { }
     *     { FieldList }
     */

    final public Node parseObjectLiteral() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseObjectLiteral" );
        }

        Node $$,$1;
    
        // {

        match( leftbrace_token ); 

        // }

        if( lookahead( rightbrace_token ) ) {

            match( rightbrace_token );
            $1 = null;

        // FieldList }

        } else {

            // Inlining parseFieldList:
            //     FieldList: LiteralField FieldListPrime

            $1 = parseLiteralField();
            $1 = parseFieldListPrime(NodeFactory.List(null,$1));

            match( rightbrace_token );

        }

        $$ = NodeFactory.LiteralObject($1);

        if( debug ) {
            Debugger.trace( "leaving parseObjectLiteral with $$ = " + $$ );
        }

        return $$;
    }

    static final void testObjectLiteral() throws Exception {

        String[] input = { 
	        "{a:0,'b':1,2:2,(d=e):3}",
        };

        String[] expected = { 
            "literalobject( list( list( list( list( null, literalfield( identifier( a ), literalnumber( 0 ) ) ), literalfield( literalstring( b ), literalnumber( 1 ) ) ), literalfield( literalnumber( 2 ), literalnumber( 2 ) ) ), literalfield( assignmentexpression( assign_token, qualifiedidentifier( null, d ), coersionexpression( qualifiedidentifier( null, e ), qualifiedidentifier( null, d ) )), literalnumber( 3 ) ) ) )",
        };

        testParser("ObjectLiteral",input,expected);
    }

    /**
     * FieldListPrime	
     *     , LiteralField FieldListPrime
     *     empty
     */

    final public Node parseFieldListPrime( Node $1 ) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseFieldListPrime with $1 = " + $1 );
        }

        Node $$;

        // , LiteralField FieldListPrime

        if( lookahead( comma_token ) ) {

            Node $2;
            match( comma_token );
            $2 = parseLiteralField();
            $$ = parseFieldListPrime(NodeFactory.List($1,$2));

        // empty

        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "leaving parseFieldListPrime with $$ = " + $$ );
        }
    
        return $$;
    }

    /**
     * LiteralField	
     *     FieldName : AssignmentExpressionallowIn
     */

    final public Node parseLiteralField() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseLiteralField" );
        }

        Node $$,$1,$2;

        $1 = parseFieldName();
        match(colon_token);
        $2 = parseAssignmentExpression(allowIn_mode);

        $$ = NodeFactory.LiteralField($1,$2);

        if( debug ) {
            Debugger.trace( "finish parseLiteralField" );
        }

        return $$;
    }

    static final void testLiteralField() throws Exception {

        String[] input = { 
	        "",
	        "" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("LiteralField",input,expected);
    }

    /**
     * FieldName	
     *     Identifier
     *     String
     *     Number
     *     ParenthesizedExpression
     */

    final public Node parseFieldName() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseFieldName" );
        }

        Node $$, $1;

        if( lookahead( stringliteral_token ) ) {
            $$ = NodeFactory.LiteralString( scanner.getTokenText(match( stringliteral_token )) );
        } else if( lookahead( numberliteral_token ) ) {
            $$ = NodeFactory.LiteralNumber(scanner.getTokenText(match(numberliteral_token)));
        } else if( lookahead( leftparen_token ) ) {
            $$ = parseParenthesizedExpression();
        } else {
            $$ = parseIdentifier();
        }

        if( debug ) {
            Debugger.trace( "finish parseFieldName" );
        }

        return $$;
    }

    static final void testFieldName() throws Exception {

        String[] input = { 
	        "",
	        "" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("FieldName",input,expected);
    }

    /**
     * ArrayLiteral	
     *     [ ElementList ]
     */

    final public Node parseArrayLiteral() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseArrayLiteral" );
        }

        Node $$, $1;
    
        match( leftbracket_token ); 

        // ElementList : LiteralElement ElementListPrime (inlined)
        
        $1 = parseLiteralElement();
        $$ = NodeFactory.LiteralArray(parseElementListPrime(NodeFactory.List(null,$1)));

        match( rightbracket_token ); 

        if( debug ) {
            Debugger.trace( "finish parseArrayLiteral" );
        }

        return $$;
    }

    static final void testArrayLiteral() throws Exception {

        String[] input = { 
	        "[]",
	        "[1,2,3]" 
        };

        String[] expected = { 
            "literalarray( list( null, null ) )",
            "literalarray( list( list( list( null, literalnumber( 1 ) ), literalnumber( 2 ) ), literalnumber( 3 ) ) )" 
        };

        testParser("ArrayLiteral",input,expected);
    }

    /**
     * ElementListPrime	
     *     , LiteralElement ElementListPrime
     *     empty
     */

    final public Node parseElementListPrime( Node $1 ) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseElementListPrime" );
        }

        Node $$;

        // , LiteralElement ElementListPrime

        if( lookahead( comma_token ) ) {

            Node $2;

            match( comma_token );
            $2 = parseLiteralElement();
            if( $2 == null ) {
                $$ = $1;
            } else {
                $$ = parseElementListPrime(NodeFactory.List($1,$2));
            }

        // empty

        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseElementListPrime" );
        }
    
        return $$;
    }

    /**
     * LiteralElement
     *     AssignmentExpression
     *     empty
     */

    final public Node parseLiteralElement() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseLiteralElement" );
        }

        Node $$;

        // empty

        if( lookahead( comma_token ) ||
            lookahead( rightbracket_token ) ) {

            $$ = null;

        // AssignmentExpression

        } else {

            $$ = parseAssignmentExpression(allowIn_mode);

        }

        if( debug ) {
            Debugger.trace( "finish parseLiteralElement" );
        }
    
        return $$;
    }

    static final void testLiteralElement() throws Exception {

        String[] input = { 
	        "",
	        "" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("LiteralElement",input,expected);
    }

    /**
     * AttributeExpression	
     *     SimpleQualifiedIdentifier AttributeExpressionPrime
     *     
     * AttributeExpressionPrime	
     *     DotOperator AttributeExpressionPrime
     *     Brackets AttributeExpressionPrime
     *     Arguments AttributeExpressionPrime
     *     «empty»
     */

    final public Node parseAttributeExpression() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseAttributeExpression" );
        }

        Node $$,$1;

        $1 = parseSimpleQualifiedIdentifier();
        $$ = parseAttributeExpressionPrime($1);

        if( debug ) {
            Debugger.trace( "finish parseAttributeExpression" );
        }
    
        return $$;
    }

    final public Node parseAttributeExpressionPrime(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseAttributeExpressionPrime" );
        }

        Node $$;

        if( lookahead( dot_token ) ) {

            $$ = parseAttributeExpressionPrime(parseDotOperator($1));

        } else if ( lookahead(leftbracket_token) ) {

            $$ = parseAttributeExpressionPrime(parseBrackets($1));

        } else if ( lookahead(leftparen_token) ) {

            $$ = parseAttributeExpressionPrime(parseArguments($1));

        } else {

            $$ = $1;

        }

        if( debug ) {
            Debugger.trace( "finish parseAttributeExpressionPrime" );
        }
    
        return $$;
    }

    static final void testAttributeExpression() throws Exception {

        String[] input = { 
	        "a::b::c.d[e](f,g)[h].i",
        };

        String[] expected = { 
            "memberexpression( indexedmemberexpression( callexpression( indexedmemberexpression( memberexpression( qualifiedidentifier( list( list( null, identifier( a ) ), identifier( b ) ), c ), qualifiedidentifier( null, d ) ), list( null, qualifiedidentifier( null, e ) ) ), list( list( null, qualifiedidentifier( null, f ) ), qualifiedidentifier( null, g ) ) ), list( null, qualifiedidentifier( null, h ) ) ), qualifiedidentifier( null, i ) )",
        };

        testParser("AttributeExpression",input,expected);
    }

    /**
     * PostfixExpression	
     *     const PostfixExpressionNonConst
     *     PostfixExpressionNonConst
     */

    final public Node parsePostfixExpression() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parsePostfixExpression" );
        }

        Node $$,$1;

        if( lookahead(const_token) ) {
            match(const_token);
            // ACTION: Do something with const.
        } 

        $$ = parsePostfixExpressionNonConst();

        if( debug ) {
            Debugger.trace( "finish parsePostfixExpression" );
        }
    
        return $$;
    }

    static final void testPostfixExpression() throws Exception {

        String[] input = { 
	        "a::b::c.d[e](f,g)[h].i",
            "const new const function f(a,b){return a+b;}.(a<<b)::c[1,2].class",
            "function f(a,b){return a+b;}",
            "(a<<b)::c",
            "new const 10 'miles'.(a<<b)::c(d,1,'z')",
            "new const {a:1,b:2,c:3}.(a<<b)::c(d,1,'z').e::f",
            "new const /xyz/gim.(a<<b)::c(d,1,'z')[e,f]",
            "new const function f(a,b){return a+b;}.(a<<b)::c(d,1,'z')(e,f)",
            "const new const (a,b,c,1+2+3) 'miles'.(a<<b)::c[1,2].class++",
            "const new const function f(a,b){return a+b;}.(a<<b)::c[1,2].class--",
            "const new const function f(a,b){return a+b;}.(a<<b)::c[1,2].class@T",
            "const new const function f(a,b){return a+b;}.(a<<b)::c[1,2].class@(T+V)",
        };

        String[] expected = { 
            "memberexpression( indexedmemberexpression( callexpression( indexedmemberexpression( memberexpression( qualifiedidentifier( list( list( null, identifier( a ) ), identifier( b ) ), c ), qualifiedidentifier( null, d ) ), list( null, qualifiedidentifier( null, e ) ) ), list( list( null, qualifiedidentifier( null, f ) ), qualifiedidentifier( null, g ) ) ), list( null, qualifiedidentifier( null, h ) ) ), qualifiedidentifier( null, i ) )",
            "newexpression( memberexpression( indexedmemberexpression( memberexpression( functionexpression( identifier( f ), functionsignature( list( parameter( identifier( a ), null ), parameter( identifier( b ), null ) ), null ), statementlist( statementlist( statementlist( statementlist( null, null:returnstatement( binaryexpression( plus_token, qualifiedidentifier( null, a ), qualifiedidentifier( null, b ) ) ) ), emptystatement ), null ), null ) ), qualifiedidentifier( binaryexpression( leftshift_token, qualifiedidentifier( null, a ), qualifiedidentifier( null, b ) ), c ) ), list( list( null, literalnumber( 1 ) ), literalnumber( 2 ) ) ), identifier( class ) ) )",
            "functionexpression( identifier( f ), functionsignature( list( parameter( identifier( a ), null ), parameter( identifier( b ), null ) ), null ), statementlist( statementlist( statementlist( statementlist( null, null:returnstatement( binaryexpression( plus_token, qualifiedidentifier( null, a ), qualifiedidentifier( null, b ) ) ) ), emptystatement ), null ), null ) )",
            "qualifiedidentifier( parenthesizedexpression( binaryexpression( leftshift_token, qualifiedidentifier( null, a ), qualifiedidentifier( null, b ) ) ), c )",
            "newexpression( callexpression( memberexpression( unitexpression( literalnumber( 10 ), literalstring( miles ) ), qualifiedidentifier( binaryexpression( leftshift_token, qualifiedidentifier( null, a ), qualifiedidentifier( null, b ) ), c ) ), list( list( list( null, qualifiedidentifier( null, d ) ), literalnumber( 1 ) ), literalstring( z ) ) ) )",
            "newexpression( memberexpression( callexpression( memberexpression( literalobject( list( list( list( null, literalfield( identifier( a ), literalnumber( 1 ) ) ), literalfield( identifier( b ), literalnumber( 2 ) ) ), literalfield( identifier( c ), literalnumber( 3 ) ) ) ), qualifiedidentifier( binaryexpression( leftshift_token, qualifiedidentifier( null, a ), qualifiedidentifier( null, b ) ), c ) ), list( list( list( null, qualifiedidentifier( null, d ) ), literalnumber( 1 ) ), literalstring( z ) ) ), qualifiedidentifier( list( null, identifier( e ) ), f ) ) )",
            "newexpression( indexedmemberexpression( callexpression( memberexpression( literalregexp( /xyz/gim ), qualifiedidentifier( binaryexpression( leftshift_token, qualifiedidentifier( null, a ), qualifiedidentifier( null, b ) ), c ) ), list( list( list( null, qualifiedidentifier( null, d ) ), literalnumber( 1 ) ), literalstring( z ) ) ), list( list( null, qualifiedidentifier( null, e ) ), qualifiedidentifier( null, f ) ) ) )",
            "newexpression( callexpression( callexpression( memberexpression( functionexpression( identifier( f ), functionsignature( list( parameter( identifier( a ), null ), parameter( identifier( b ), null ) ), null ), statementlist( statementlist( statementlist( statementlist( null, null:returnstatement( binaryexpression( plus_token, qualifiedidentifier( null, a ), qualifiedidentifier( null, b ) ) ) ), emptystatement ), null ), null ) ), qualifiedidentifier( binaryexpression( leftshift_token, qualifiedidentifier( null, a ), qualifiedidentifier( null, b ) ), c ) ), list( list( list( null, qualifiedidentifier( null, d ) ), literalnumber( 1 ) ), literalstring( z ) ) ), list( list( null, qualifiedidentifier( null, e ) ), qualifiedidentifier( null, f ) ) ) )",
            "newexpression( postfixexpression( plusplus_token, memberexpression( indexedmemberexpression( memberexpression( unitexpression( parenthesizedexpression( list( list( list( qualifiedidentifier( null, a ), qualifiedidentifier( null, b ) ), qualifiedidentifier( null, c ) ), binaryexpression( plus_token, binaryexpression( plus_token, literalnumber( 1 ), literalnumber( 2 ) ), literalnumber( 3 ) ) ) ), literalstring( miles ) ), qualifiedidentifier( binaryexpression( leftshift_token, qualifiedidentifier( null, a ), qualifiedidentifier( null, b ) ), c ) ), list( list( null, literalnumber( 1 ) ), literalnumber( 2 ) ) ), identifier( class ) ) ) )",
            "newexpression( postfixexpression( minusminus_token, memberexpression( indexedmemberexpression( memberexpression( functionexpression( identifier( f ), functionsignature( list( parameter( identifier( a ), null ), parameter( identifier( b ), null ) ), null ), statementlist( statementlist( statementlist( statementlist( null, null:returnstatement( binaryexpression( plus_token, qualifiedidentifier( null, a ), qualifiedidentifier( null, b ) ) ) ), emptystatement ), null ), null ) ), qualifiedidentifier( binaryexpression( leftshift_token, qualifiedidentifier( null, a ), qualifiedidentifier( null, b ) ), c ) ), list( list( null, literalnumber( 1 ) ), literalnumber( 2 ) ) ), identifier( class ) ) ) )",
            "newexpression( coersionexpression( memberexpression( indexedmemberexpression( memberexpression( functionexpression( identifier( f ), functionsignature( list( parameter( identifier( a ), null ), parameter( identifier( b ), null ) ), null ), statementlist( statementlist( statementlist( statementlist( null, null:returnstatement( binaryexpression( plus_token, qualifiedidentifier( null, a ), qualifiedidentifier( null, b ) ) ) ), emptystatement ), null ), null ) ), qualifiedidentifier( binaryexpression( leftshift_token, qualifiedidentifier( null, a ), qualifiedidentifier( null, b ) ), c ) ), list( list( null, literalnumber( 1 ) ), literalnumber( 2 ) ) ), identifier( class ) ), qualifiedidentifier( null, T ) ) )",
            "newexpression( coersionexpression( memberexpression( indexedmemberexpression( memberexpression( functionexpression( identifier( f ), functionsignature( list( parameter( identifier( a ), null ), parameter( identifier( b ), null ) ), null ), statementlist( statementlist( statementlist( statementlist( null, null:returnstatement( binaryexpression( plus_token, qualifiedidentifier( null, a ), qualifiedidentifier( null, b ) ) ) ), emptystatement ), null ), null ) ), qualifiedidentifier( binaryexpression( leftshift_token, qualifiedidentifier( null, a ), qualifiedidentifier( null, b ) ), c ) ), list( list( null, literalnumber( 1 ) ), literalnumber( 2 ) ) ), identifier( class ) ), binaryexpression( plus_token, qualifiedidentifier( null, T ), qualifiedidentifier( null, V ) ) ) )",
        };

        testParser("PostfixExpression",input,expected);
    }
    /**
     * PostfixExpressionNonConst	
     *     NewPostfixExpression
     *     DotPostfixExpression
     */

    final public Node parsePostfixExpressionNonConst() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parsePostfixExpressionNonConst" );
        }

        Node $$,$1;

        if( lookahead(new_token) ) {
            $$ = parseNewPostfixExpression();
        } else {
            $$ = parseDotPostfixExpression();
        }

        if( debug ) {
            Debugger.trace( "finish parsePostfixExpressionNonConst" );
        }
    
        return $$;
    }

    /**
     * NewPostfixExpression	
     *     new ShortNewSubexpression
     *     new ShortNewSubexpression {$1.isConstExpression}  BracketExpressionSuffix Arguments FullPostfixDotSubexpressionPrime
     *     new ShortNewSubexpression PostfixExpressionSuffixOne FullPostfixSubexpressionPrime
     */

    final public Node parseNewPostfixExpression() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseNewPostfixExpression" );
        }

        Node $$,$1;

        match(new_token);

        $1 = parseShortNewSubexpression();
        
        if( lookahead(leftbracket_token) ||
            lookahead(leftparen_token) ) {

            // ACTION: verify that $1 is a ConstExpression here.

            $1 = parseBracketExpressionSuffix($1);
            $1 = parseArguments($1);
            $1 = parseFullPostfixDotSubexpressionPrime($1);
        } else if( lookahead(plusplus_token) ||
            lookahead(minusminus_token) ||
            lookahead(ampersand_token) ) {
            $1 = parsePostfixExpressionSuffixOne($1);
            $1 = parseFullPostfixSubexpressionPrime($1);
        } else {
            $1 = $1;
        }

        $$ = NodeFactory.NewExpression($1);

        if( debug ) {
            Debugger.trace( "finish parseNewPostfixExpression" );
        }
    
        return $$;
    }

    /**
     * DotPostfixExpression	
     *     PrimaryExpression FullPostfixDotSubexpressionPrime
     *     QualifiedIdentifier FullPostfixDotSubexpressionPrime
     */

    final public Node parseDotPostfixExpression() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseDotPostfixExpression" );
        }

        Node $$,$1;

        if( lookahead(identifier_token) ||
            lookahead(get_token) ||
            lookahead(set_token) ||
            lookahead(public_token) ||
            lookahead(private_token) ||
            lookahead(super_token) ) {
            $1 = parseSimpleQualifiedIdentifier();
        } else {
            $1 = parsePrimaryExpressionOrExpressionQualifiedIdentifier();
        }

        $$ = parseFullPostfixDotSubexpressionPrime($1);

        if( debug ) {
            Debugger.trace( "finish parseDotPostfixExpression" );
        }
    
        return $$;
    }

    /**
     * FullPostfixDotSubexpressionPrime	
     *     DotExpressionPrime
     *     DotExpressionPrime PostfixExpressionSuffixTwo FullPostfixSubexpressionPrime
     */

    final public Node parseFullPostfixDotSubexpressionPrime(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseFullPostfixDotSubexpressionPrime" );
        }

        Node $$;

        $1 = parseDotExpressionPrime($1);
        
        if( lookahead(leftbracket_token) ||
            lookahead(leftparen_token) ||
            lookahead(plusplus_token) ||
            lookahead(minusminus_token) ||
            lookahead(ampersand_token) ) { 
            $1 = parsePostfixExpressionSuffixTwo($1);
            $$ = parseFullPostfixSubexpressionPrime($1);
        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseFullPostfixDotSubexpressionPrime" );
        }
    
        return $$;
    }

    /**
     * FullPostfixSubexpressionPrime	
     *     «empty»
     *     PostfixExpressionSuffixThree FullPostfixSubexpressionPrime
     */

    final public Node parseFullPostfixSubexpressionPrime(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseFullPostfixSubexpressionPrime" );
        }

        Node $$;

        if( lookahead(dot_token) ||
            lookahead(leftbracket_token) ||
            lookahead(leftparen_token) ||
            lookahead(plusplus_token) ||
            lookahead(minusminus_token) ||
            lookahead(ampersand_token) ) {
            $$ = parseFullPostfixSubexpressionPrime(parsePostfixExpressionSuffixThree($1));
        } else {
            $$ = $1;
        }


        if( debug ) {
            Debugger.trace( "finish parseFullPostfixSubexpressionPrime" );
        }
    
        return $$;
    }

    /**
     * PostfixExpressionSuffixOne	
     *     [no line break] ++
     *     [no line break] --
     *     @ QualifiedIdentifier
     *     @ ParenthesizedExpression
     */

    final public Node parsePostfixExpressionSuffixOne(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parsePostfixExpressionSuffixOne" );
        }

        Node $$;

        if( lookahead(plusplus_token) ) {
            if( newline() ) {
                throw new Exception("Newline not allowed before postfix increment operator.");
            }
            $$ = NodeFactory.PostfixExpression(match(plusplus_token),$1);
        } else if( lookahead(minusminus_token) ) {
            if( newline() ) {
                throw new Exception("Newline not allowed before postfix decrement operator.");
            }
            $$ = NodeFactory.PostfixExpression(match(minusminus_token),$1);
        } else {
            match(ampersand_token);
            if( lookahead(leftparen_token) ) {
                $$ = NodeFactory.CoersionExpression($1,parseParenthesizedExpression());
            } else {
                $$ = NodeFactory.CoersionExpression($1,parseQualifiedIdentifier());
            }
        }

        if( debug ) {
            Debugger.trace( "finish parsePostfixExpressionSuffixOne" );
        }
    
        return $$;
    }

    /**
     * PostfixExpressionSuffixTwo	
     *     Brackets
     *     Arguments
     *     PostfixExpressionSuffixOne
     */

    final public Node parsePostfixExpressionSuffixTwo(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parsePostfixExpressionSuffixTwo" );
        }

        Node $$;

        if( lookahead(leftbracket_token) ) {
            $$ = parseBrackets($1);
        } else if( lookahead(leftparen_token) ) {
            $$ = parseArguments($1);
        } else {
            $$ = parsePostfixExpressionSuffixOne($1);
        }


        if( debug ) {
            Debugger.trace( "finish parsePostfixExpressionSuffixTwo" );
        }
    
        return $$;
    }

    /**
     * PostfixExpressionSuffixThree	
     *     DotOperator
     *     PostfixExpressionSuffixTwo
     */

    final public Node parsePostfixExpressionSuffixThree(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parsePostfixExpressionSuffixThree" );
        }

        Node $$;

        if( lookahead(dot_token) ) {
            $$ = parseDotOperator($1);
        } else {
            $$ = parsePostfixExpressionSuffixTwo($1);
        }


        if( debug ) {
            Debugger.trace( "finish parsePostfixExpressionSuffixThree" );
        }
    
        return $$;
    }

    /**
     * FullNewExpression	
     *     new BracketExpression Arguments
     */

    final public Node parseFullNewExpression() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseFullNewExpression" );
        }

        Node $$,$1;

        match(new_token);
        $1 = parseBracketExpression();
        $$ = parseArguments($1);
        $$ = NodeFactory.NewExpression($1);

        if( debug ) {
            Debugger.trace( "finish parseFullNewExpression" );
        }
    
        return $$;
    }

    /**
     * ShortNewExpression	
     *     new ShortNewSubexpression
     *     const new ShortNewSubexpression
     */

    final public Node parseShortNewExpression() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseShortNewExpression" );
        }

        Node $$,$1;

        if( lookahead(const_token) ) {
            match(const_token);
            // Do something with this.
        }

        match(new_token);
        $1 = parseShortNewSubexpression();
        $$ = NodeFactory.NewExpression($1);

        if( debug ) {
            Debugger.trace( "finish parseShortNewExpression" );
        }
    
        return $$;
    }

    /**
     * ShortNewSubexpression	
     *     const ShortNewSubexpressionNonConst
     *     ShortNewSubexpressionNonConst
     */

    final public Node parseShortNewSubexpression() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseShortNewSubexpression" );
        }

        Node $$,$1;

        if( lookahead(const_token) ) {
            match(const_token);
            // Do something with this.
        }

        $$ = parseShortNewSubexpressionNonConst();

        if( debug ) {
            Debugger.trace( "finish parseShortNewSubexpression" );
        }
    
        return $$;
    }

    /**
     * ShortNewSubexpressionNonConst	
     *     new ShortNewSubexpression {$1.isConstExpression} BracketExpressionSuffix Arguments DotExpressionPrime BracketExpressionSuffix
     *     new ShortNewSubexpression
     *     PrimaryExpression DotExpressionPrime BracketExpressionSuffix
     *     ExpressionQualifiedIdentifier DotExpressionPrime BracketExpressionSuffix
     *     SimpleQualifiedIdentifier DotExpressionPrime BracketExpressionSuffix
     */

    final public Node parseShortNewSubexpressionNonConst() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseShortNewSubexpressionNonConst" );
        }

        Node $$,$1;

        if( lookahead(new_token) ) {
            match(new_token);
            $1 = parseShortNewSubexpression();
            if( lookahead(leftbracket_token) ||
                lookahead(leftparen_token) ) {
                $1 = parseBracketExpressionSuffix($1);
                $1 = parseArguments($1);
                $1 = parseDotExpressionPrime($1);
                $1 = parseBracketExpressionSuffix($1);
            } else {
            }
            $$ = NodeFactory.NewExpression($1);
        } else {
            if( lookahead(identifier_token) ||
                lookahead(get_token) ||
                lookahead(set_token) ||
                lookahead(public_token) ||
                lookahead(private_token) ||
                lookahead(super_token) ) {
                $1 = parseSimpleQualifiedIdentifier();
            } else {
                $1 = parsePrimaryExpressionOrExpressionQualifiedIdentifier();
            }

            $1 = parseDotExpressionPrime($1);
            $$ = parseBracketExpressionSuffix($1);
        }

        if( debug ) {
            Debugger.trace( "finish parseShortNewSubexpressionNonConst" );
        }
    
        return $$;
    }

    /**
     * BracketExpressionSuffix	
     *     empty
     *     Brackets BracketSubexpressionPrime
     */

    final public Node parseBracketExpressionSuffix(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseBracketExpressionSuffix" );
        }

        Node $$;

        if( lookahead(leftbracket_token) ) {
            $1 = parseBrackets($1);
            $$ = parseBracketSubexpressionPrime($1);
        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseBracketExpressionSuffix" );
        }
    
        return $$;
    }

    /**
     * BracketExpression	
     *     ConstExpression
     *     ConstExpression Brackets BracketSubexpressionPrime
     */

    final public Node parseBracketExpression() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseBracketExpression" );
        }

        Node $$,$1;

        $1 = parseConstExpression();
        if( lookahead(leftbracket_token) ) {
            $$ = parseBracketSubexpressionPrime(parseBrackets($1));
        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseBracketExpression" );
        }
    
        return $$;
    }

    static final void testBracketExpression() throws Exception {

        String[] input = { 
            "const function f(a,b){return a+b;}.(a<<b)::c",
            "const function f(a,b){return a+b;}.(a<<b)::c[1,2].class",
        };

        String[] expected = { 
            "",
            "",
        };

        testParser("BracketExpression",input,expected);
    }

    /**
     * BracketSubexpression	
     *     ConstExpression Brackets BracketSubexpressionPrime
     * 	
     * BracketSubexpressionPrime	
     *     «empty»
     *     Brackets BracketSubexpressionPrime
     *     DotOperator BracketSubexpressionPrime
     */

    final public Node parseBracketSubexpression() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseBracketSubexpression" );
        }

        Node $$;

        $$ = parseBracketSubexpressionPrime(parseBrackets(parseConstExpression()));

        if( debug ) {
            Debugger.trace( "finish parseBracketSubexpression" );
        }
    
        return $$;
    }

    final public Node parseBracketSubexpressionPrime(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseBracketSubexpressionPrime" );
        }

        Node $$;

        if( lookahead(dot_token) ) {
            $$ = parseBracketSubexpressionPrime(parseDotOperator($1));
        } else if( lookahead(leftbracket_token) ) {
            $$ = parseBracketSubexpressionPrime(parseBrackets($1));
        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseBracketSubexpressionPrime" );
        }
    
        return $$;
    }

    static final void testBracketSubexpression() throws Exception {

        String[] input = { 
            "const function f(a,b){return a+b;}.(a<<b)::c[1,2]",
            "const function f(a,b){return a+b;}.(a<<b)::c[1,2].class",
        };

        String[] expected = { 
            "",
            "",
        };

        testParser("BracketSubexpression",input,expected);
    }

    /**
     * ConstExpression	
     *     DotExpression
     *     ConstDotExpression
     */

    final public Node parseConstExpression() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseConstExpression" );
        }

        Node $$;

        if( lookahead(const_token) ) {
            $$ = parseConstDotExpression();
        } else {
            $$ = parseDotExpression();
        }

        if( debug ) {
            Debugger.trace( "finish parseConstExpression" );
        }
    
        return $$;
    }

    static final void testConstExpression() throws Exception {

        String[] input = { 
            "function f(a,b){return a+b;}.(a<<b)::c",
            "const function f(a,b){return a+b;}.(a<<b)::c",
        };

        String[] expected = { 
            "",
            "",
        };

        testParser("ConstExpression",input,expected);
    }

    /**
     * ConstDotExpression	
     *     const DotExpression
     */

    final public Node parseConstDotExpression() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseConstDotExpression" );
        }

        Node $$;

        match(const_token);
        $$ = parseDotExpression();

        if( debug ) {
            Debugger.trace( "finish parseConstDotExpression" );
        }
    
        return $$;
    }

    static final void testConstDotExpression() throws Exception {

        String[] input = { 
            "const function f(a,b){return a+b;}.(a<<b)::c",
        };

        String[] expected = { 
            "",
        };

        testParser("ConstDotExpression",input,expected);
    }

    /**
     * DotExpression	
     *     SimpleQualifiedIdentifier DotExpressionPrime
     *     PrimaryExpression DotExpressionPrime
     *     ExpressionQualifiedIdentifier DotExpressionPrime
     *     FullNewExpression DotExpressionPrime
     * 	
     * DotExpressionPrime	
     *     «empty»
     *     DotOperator DotExpressionPrime
     */

    final public Node parseDotExpression() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseDotExpression" );
        }

        Node $$,$1;

        if( lookahead(new_token) ) {
            $1 = parseFullNewExpression();
        } else if( lookahead(identifier_token) ||
                   lookahead(get_token) ||
                   lookahead(set_token) ||
                   lookahead(public_token) ||
                   lookahead(private_token) ||
                   lookahead(super_token) ) {
            $1 = parseSimpleQualifiedIdentifier();
        } else {
            $1 = parsePrimaryExpressionOrExpressionQualifiedIdentifier();
        }

        $$ = parseDotExpressionPrime($1);

        if( debug ) {
            Debugger.trace( "finish parseDotExpression" );
        }
    
        return $$;
    }

    final public Node parseDotExpressionPrime(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseDotExpressionPrime" );
        }

        Node $$;

        if( lookahead(dot_token) ) {
            $$ = parseDotExpressionPrime(parseDotOperator($1));
        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseDotExpressionPrime" );
        }
    
        return $$;
    }

    static final void testDotExpression() throws Exception {

        String[] input = { 
            "",
        };

        String[] expected = { 
            "",
        };

        testParser("DotExpression",input,expected);
    }

    /**
     * DotOperator	
     *     . QualifiedIdentifier
     *     . class
     */

    final public Node parseDotOperator(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseDotOperator" );
        }

        Node $$,$2;

        match(dot_token);
        if( lookahead(class_token) ) {
            match(class_token);
            $$ = NodeFactory.ClassofExpression($1);
        } else {
		    int mark = scanner.input.positionOfMark();
            $2 = parseQualifiedIdentifier();
			$2.position = mark;
            $$ = NodeFactory.MemberExpression($1,$2);
        }

        if( debug ) {
            Debugger.trace( "finish parseDotOperator" );
        }
    
        return $$;
    }

    /**
     * Brackets	
     *     [ ArgumentList ]
     */

    final public Node parseBrackets(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseBrackets" );
        }

        Node $$;
 
        match(leftbracket_token);
        $$ = NodeFactory.IndexedMemberExpression($1,parseArgumentList());
        match(rightbracket_token);

        if( debug ) {
            Debugger.trace( "finish parseBrackets $$ = " + $$ );
        }

        return $$;
    }

    static final void testBrackets() throws Exception {

        String[] input = { 
	        "",
        };

        String[] expected = { 
            "",
        };

        testParser("Brackets",input,expected);
    }

    /**
     * Arguments	
     *     ( ArgumentList )
     */

    final public Node parseArguments(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseArguments" );
        }

        Node $$;

        match(leftparen_token);
        $$ = NodeFactory.CallExpression($1,parseArgumentList());
        match(rightparen_token);

        if( debug ) {
            Debugger.trace( "finish parseArguments" );
        }

        return $$;
    }

    static final void testArguments() throws Exception {

        String[] input = { 
	        "",
	        "" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("Arguments",input,expected);
    }

    /**
     * ArgumentList	
     *     «empty»
     *     LiteralField NamedArgumentListPrime
     *     AssignmentExpression[allowIn] ArgumentListPrime
     */

    final public Node parseArgumentList() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseArgumentList" );
        }

        Node $$;
         
        if( lookahead(rightparen_token) || lookahead(rightbracket_token) ) {

            $$ = null;

        } else {

            Node $1;
            $1 = parseAssignmentExpression(allowIn_mode);
            if( AssignmentExpressionNode.isFieldName($1) && lookahead(colon_token) ) {

                match(colon_token);

                // Finish parsing the LiteralField.
                
                $1 = NodeFactory.LiteralField($1,parseAssignmentExpression(allowIn_mode));
                $$ = parseNamedArgumentListPrime(NodeFactory.List(null,$1));

            } else {

                $$ = parseArgumentListPrime(NodeFactory.List(null,$1));

            }
        }
    
        if( debug ) {
            Debugger.trace( "finish parseArgumentList $$ = " + $$ );
        }

        return $$;
    }

    static final void testArgumentList() throws Exception {

        String[] input = { 
	        "x,y,z",
	        "a:1,b:2,c:3", 
	        "a=a:1", 
	        "a:1,b:2,c:3", 
        };

        String[] expected = { 
            "list( list( qualifiedidentifier( null, x ), qualifiedidentifier( null, y ) ), qualifiedidentifier( null, z ) )",
            "list( list( literalfield( qualifiedidentifier( null, a ), literalnumber( 1 ) ), literalfield( qualifiedidentifier( null, b ), literalnumber( 2 ) ) ), literalfield( qualifiedidentifier( null, c ), literalnumber( 3 ) ) )",
            "Named argument name must be an identifier.",
            "list( list( literalfield( qualifiedidentifier( null, a ), literalnumber( 1 ) ), literalfield( qualifiedidentifier( null, b ), literalnumber( 2 ) ) ), literalfield( qualifiedidentifier( null, c ), literalnumber( 3 ) ) )",
        };

        testParser("ArgumentList",input,expected);
    }

    /**
     * ArgumentListPrime	
     *     «empty»
     *     , AssignmentExpression[allowIn] ArgumentListPrime
     *     , LiteralField NamedArgumentListPrime
     */

    final public Node parseArgumentListPrime(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseArgumentListPrime" );
        }

        Node $$;

        if( lookahead(comma_token) ) {

            match(comma_token);
            Node $2;
            $2 = parseAssignmentExpression(allowIn_mode);
            if( AssignmentExpressionNode.isFieldName($2) && lookahead(colon_token) ) {
                match(colon_token);

                // Finish parsing the literal field.
                
                $2 = NodeFactory.LiteralField($2,parseAssignmentExpression(allowIn_mode));
                $$ = parseNamedArgumentListPrime(NodeFactory.List($1,$2));

            } else {

                $$ = parseArgumentListPrime(NodeFactory.List($1,$2));

            }

        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseArgumentListPrime" );
        }

        return $$;
    }

    /**
     * NamedArgumentListPrime	
     *     «empty»
     *     , LiteralField NamedArgumentListPrime
     */

    final public Node parseNamedArgumentListPrime(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseNamedArgumentListPrime" );
        }

        Node $$;

        if( lookahead(comma_token) ) {
            match(comma_token);
            Node $2;
            $2 = parseLiteralField();
            $$ = parseNamedArgumentListPrime(NodeFactory.List($1,$2));
        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseNamedArgumentListPrime" );
        }

        return $$;
    }

    /**
     * UnaryExpression	
     *     PostfixExpression
     *     delete PostfixExpression
     *     void UnaryExpression
     *     typeof UnaryExpression
     *     ++ PostfixExpression
     *     -- PostfixExpression
     *     + UnaryExpression
     *     - UnaryExpression
     *     ~ UnaryExpression
     *     ! UnaryExpression
     */

    final public Node parseUnaryExpression() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseUnaryExpression" );
        }

        Node $$;

        if( lookahead(delete_token) ) {
            match(delete_token);
            $$ = NodeFactory.UnaryExpression(delete_token,parsePostfixExpression());
        } else if( lookahead(void_token) ) {
            match(void_token);
            $$ = NodeFactory.UnaryExpression(void_token,parseUnaryExpression());
        } else if( lookahead(typeof_token) ) {
            match(typeof_token);
            $$ = NodeFactory.UnaryExpression(typeof_token,parseUnaryExpression());
        } else if( lookahead(plusplus_token) ) {
            match(plusplus_token);
            $$ = NodeFactory.UnaryExpression(plusplus_token,parseUnaryExpression());
        } else if( lookahead(minusminus_token) ) {
            match(minusminus_token);
            $$ = NodeFactory.UnaryExpression(minusminus_token,parseUnaryExpression());
        } else if( lookahead(plus_token) ) {
            match(plus_token);
            $$ = NodeFactory.UnaryExpression(plus_token,parseUnaryExpression());
        } else if( lookahead(minus_token) ) {
            match(minus_token);
            $$ = NodeFactory.UnaryExpression(minus_token,parseUnaryExpression());
        } else if( lookahead(bitwisenot_token) ) {
            match(bitwisenot_token);
            $$ = NodeFactory.UnaryExpression(bitwisenot_token,parseUnaryExpression());
        } else if( lookahead(not_token) ) {
            match(not_token);
            $$ = NodeFactory.UnaryExpression(not_token,parseUnaryExpression());
        } else {
            $$ = parsePostfixExpression();
        }
        
        if( debug ) {
            Debugger.trace( "finish parseUnaryExpression" );
        }

        return $$;
    }

    static final void testUnaryExpression() throws Exception {

        String[] input = { 
	        "delete x",
	        "typeof x",
            "eval x",
            "++x",
            "--x",
            "+x",
            "-x",
            "~x",
            "~~x",
            "!x",
            "typeof !x",
        };

        String[] expected = { 
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
            "",
        };

        testParser("UnaryExpression",input,expected);
    }

    /**
     * MultiplicativeExpression	
     *     UnaryExpression
     *     MultiplicativeExpression * UnaryExpression
     *     MultiplicativeExpression / UnaryExpression
     *     MultiplicativeExpression % UnaryExpression
     */

    final public Node parseMultiplicativeExpression() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseMultiplicativeExpression" );
        }

        Node $$,$1;

        scanner.enterSlashDivContext();

        try {
            $1 = parseUnaryExpression();
            $$ = parseMultiplicativeExpressionPrime($1);
        } finally {
            scanner.exitSlashDivContext();
        }

        
        if( debug ) {
            Debugger.trace( "finish parseMultiplicativeExpression" );
        }

        return $$;
    }

    final public Node parseMultiplicativeExpressionPrime(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseMultiplicativeExpressionPrime" );
        }

        Node $$;

        if( lookahead(mult_token) ) {
            match(mult_token);
            $1 = NodeFactory.BinaryExpression(mult_token,$1,parseUnaryExpression());
            $$ = parseMultiplicativeExpressionPrime($1);
        } else if( lookahead(div_token) ) {
            match(div_token);
            $1 = NodeFactory.BinaryExpression(div_token,$1,parseUnaryExpression());
            $$ = parseMultiplicativeExpressionPrime($1);
        } else if( lookahead(modulus_token) ) {
            match(modulus_token);
            $1 = NodeFactory.BinaryExpression(modulus_token,$1,parseUnaryExpression());
            $$ = parseMultiplicativeExpressionPrime($1);
        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseMultiplicativeExpressionPrime" );
        }

        return $$;
    }

    static final void testMultiplicativeExpression() throws Exception {

        String[] input = { 
	        "1*2;",
	        "x/y;",
	        "x%y;",
	        "new C;",
        };

        String[] expected = { 
            "binaryexpression( mult_token, literalnumber( 1 ), literalnumber( 2 ) )",
            "binaryexpression( div_token, qualifiedidentifier( null, x ), qualifiedidentifier( null, y ) )",
            "binaryexpression( modulus_token, qualifiedidentifier( null, x ), qualifiedidentifier( null, y ) )",
            "newexpression( qualifiedidentifier( null, C ) )",
        };

        testParser("MultiplicativeExpression",input,expected);
    }

    /**
     * AdditiveExpression	
     *     MultiplicativeExpression
     *     AdditiveExpression + MultiplicativeExpression
     *     AdditiveExpression - MultiplicativeExpression
     */

    final public Node parseAdditiveExpression() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseAdditiveExpression" );
        }

        Node $$,$1;

        $1 = parseMultiplicativeExpression();
        $$ = parseAdditiveExpressionPrime($1);
        
        if( debug ) {
            Debugger.trace( "finish parseAdditiveExpression" );
        }

        return $$;
    }

    final public Node parseAdditiveExpressionPrime(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseAdditiveExpressionPrime" );
        }

        Node $$;

        if( lookahead(plus_token) ) {
            match(plus_token);
            $1 = NodeFactory.BinaryExpression(plus_token,$1,parseMultiplicativeExpression());
            $$ = parseAdditiveExpressionPrime($1);
        } else if( lookahead(minus_token) ) {
            match(minus_token);
            $1 = NodeFactory.BinaryExpression(minus_token,$1,parseMultiplicativeExpression());
            $$ = parseAdditiveExpressionPrime($1);
        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseAdditiveExpressionPrime" );
        }

        return $$;
    }

    static final void testAdditiveExpression() throws Exception {

        String[] input = { 
	        "1+2;",
	        "x-y;",
        };

        String[] expected = { 
            "",
            "",
        };

        testParser("AdditiveExpression",input,expected);
    }

    /**
     * ShiftExpression	
     *     AdditiveExpression
     *     ShiftExpression << AdditiveExpression
     *     ShiftExpression >> AdditiveExpression
     *     ShiftExpression >>> AdditiveExpression
     */

    final public Node parseShiftExpression() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseShiftExpression" );
        }

        Node $$,$1;

        $1 = parseAdditiveExpression();
        $$ = parseShiftExpressionPrime($1);
        
        if( debug ) {
            Debugger.trace( "finish parseShiftExpression" );
        }

        return $$;
    }

    final public Node parseShiftExpressionPrime(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseShiftExpressionPrime" );
        }

        Node $$;

        if( lookahead(leftshift_token) ) {
            match(leftshift_token);
            $1 = NodeFactory.BinaryExpression(leftshift_token,$1,parseAdditiveExpression());
            $$ = parseShiftExpressionPrime($1);
        } else if( lookahead(rightshift_token) ) {
            match(rightshift_token);
            $1 = NodeFactory.BinaryExpression(rightshift_token,$1,parseAdditiveExpression());
            $$ = parseShiftExpressionPrime($1);
        } else if( lookahead(unsignedrightshift_token) ) {
            match(unsignedrightshift_token);
            $1 = NodeFactory.BinaryExpression(unsignedrightshift_token,$1,parseAdditiveExpression());
            $$ = parseShiftExpressionPrime($1);
        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseShiftExpressionPrime" );
        }

        return $$;
    }

    /**
     * RelationalExpression[allowIn|noIn]
     *     ShiftExpression
     *     RelationalExpression[allowIn] < ShiftExpression
     *     RelationalExpression[allowIn] > ShiftExpression
     *     RelationalExpression[allowIn] <= ShiftExpression
     *     RelationalExpression[allowIn] >= ShiftExpression
     *     RelationalExpression[allowIn] instanceof ShiftExpression
     *     RelationalExpression[allowIn] in ShiftExpression
     */

    final public Node parseRelationalExpression(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseRelationalExpression" );
        }

        Node $$,$1;

        $1 = parseShiftExpression();
        $$ = parseRelationalExpressionPrime(mode,$1);
        
        if( debug ) {
            Debugger.trace( "finish parseRelationalExpression" );
        }

        return $$;
    }

    final public Node parseRelationalExpressionPrime(int mode, Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseRelationalExpressionPrime" );
        }

        Node $$;

        if( lookahead(lessthan_token) ) {
            match(lessthan_token);
            $1 = NodeFactory.BinaryExpression(lessthan_token,$1,parseShiftExpression());
            $$ = parseRelationalExpressionPrime(mode,$1);
        } else if( lookahead(greaterthan_token) ) {
            match(greaterthan_token);
            $1 = NodeFactory.BinaryExpression(greaterthan_token,$1,parseShiftExpression());
            $$ = parseRelationalExpressionPrime(mode,$1);
        } else if( lookahead(lessthanorequals_token) ) {
            match(lessthanorequals_token);
            $1 = NodeFactory.BinaryExpression(lessthanorequals_token,$1,parseShiftExpression());
            $$ = parseRelationalExpressionPrime(mode,$1);
        } else if( lookahead(greaterthanorequals_token) ) {
            match(greaterthanorequals_token);
            $1 = NodeFactory.BinaryExpression(greaterthanorequals_token,$1,parseShiftExpression());
            $$ = parseRelationalExpressionPrime(mode,$1);
        } else if( lookahead(instanceof_token) ) {
            match(instanceof_token);
            $1 = NodeFactory.BinaryExpression(instanceof_token,$1,parseShiftExpression());
            $$ = parseRelationalExpressionPrime(mode,$1);
        } else if( mode == allowIn_mode && lookahead(in_token) ) {
            match(in_token);
            $1 = NodeFactory.BinaryExpression(in_token,$1,parseShiftExpression());
            $$ = parseRelationalExpressionPrime(mode,$1);
        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseRelationalExpressionPrime" );
        }

        return $$;
    }

    /**
     * EqualityExpression	
     *     RelationalExpression
     *     EqualityExpression == RelationalExpression
     *     EqualityExpression != RelationalExpression
     *     EqualityExpression === RelationalExpression
     *     EqualityExpression !== RelationalExpression
     */

    final public Node parseEqualityExpression(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseEqualityExpression" );
        }

        Node $$,$1;

        $1 = parseRelationalExpression(mode);
        $$ = parseEqualityExpressionPrime(mode,$1);
        
        if( debug ) {
            Debugger.trace( "finish parseEqualityExpression" );
        }

        return $$;
    }

    final public Node parseEqualityExpressionPrime(int mode, Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseEqualityExpressionPrime" );
        }

        Node $$;

        if( lookahead(equals_token) ) {
            match(equals_token);
            $1 = NodeFactory.BinaryExpression(equals_token,$1,parseRelationalExpression(mode));
            $$ = parseEqualityExpressionPrime(mode,$1);
        } else if( lookahead(notequals_token) ) {
            match(notequals_token);
            $1 = NodeFactory.BinaryExpression(notequals_token,$1,parseRelationalExpression(mode));
            $$ = parseEqualityExpressionPrime(mode,$1);
        } else if( lookahead(strictequals_token) ) {
            match(strictequals_token);
            $1 = NodeFactory.BinaryExpression(strictequals_token,$1,parseRelationalExpression(mode));
            $$ = parseEqualityExpressionPrime(mode,$1);
        } else if( lookahead(strictnotequals_token) ) {
            match(strictnotequals_token);
            $1 = NodeFactory.BinaryExpression(strictnotequals_token,$1,parseRelationalExpression(mode));
            $$ = parseEqualityExpressionPrime(mode,$1);
        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseEqualityExpressionPrime" );
        }

        return $$;
    }

    /**
     * BitwiseAndExpression	
     *     EqualityExpression
     *     BitwiseAndExpression & EqualityExpression
     */

    final public Node parseBitwiseAndExpression(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseBitwiseAndExpression" );
        }

        Node $$,$1;

        $1 = parseEqualityExpression(mode);
        $$ = parseBitwiseAndExpressionPrime(mode,$1);
        
        if( debug ) {
            Debugger.trace( "finish parseBitwiseAndExpression" );
        }

        return $$;
    }

    final public Node parseBitwiseAndExpressionPrime(int mode, Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseBitwiseAndExpressionPrime" );
        }

        Node $$;

        if( lookahead(bitwiseand_token) ) {
            match(bitwiseand_token);
            $1 = NodeFactory.BinaryExpression(bitwiseand_token,$1,parseEqualityExpression(mode));
            $$ = parseBitwiseAndExpressionPrime(mode,$1);
        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseBitwiseAndExpressionPrime" );
        }

        return $$;
    }

    /**
     * BitwiseXorExpression	
     *     BitwiseAndExpression
     *     BitwiseXorExpression ^ BitwiseAndExpression
     */

    final public Node parseBitwiseXorExpression(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseBitwiseXorExpression" );
        }

        Node $$,$1;

        $1 = parseBitwiseAndExpression(mode);
        $$ = parseBitwiseXorExpressionPrime(mode,$1);
        
        if( debug ) {
            Debugger.trace( "finish parseBitwiseXorExpression" );
        }

        return $$;
    }

    final public Node parseBitwiseXorExpressionPrime(int mode, Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseBitwiseXorExpressionPrime" );
        }

        Node $$;

        if( lookahead(bitwisexor_token) ) {
            match(bitwisexor_token);
            $1 = NodeFactory.BinaryExpression(bitwisexor_token,$1,parseBitwiseAndExpression(mode));
            $$ = parseBitwiseXorExpressionPrime(mode,$1);
        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseBitwiseXorExpressionPrime" );
        }

        return $$;
    }

    /**
     * BitwiseOrExpression	
     *     BitwiseXorExpression
     *     BitwiseOrExpression | BitwiseXorExpression
     */

    final public Node parseBitwiseOrExpression(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseBitwiseOrExpression" );
        }

        Node $$,$1;

        $1 = parseBitwiseXorExpression(mode);
        $$ = parseBitwiseOrExpressionPrime(mode,$1);
        
        if( debug ) {
            Debugger.trace( "finish parseBitwiseOrExpression" );
        }

        return $$;
    }

    final public Node parseBitwiseOrExpressionPrime(int mode, Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseBitwiseOrExpressionPrime" );
        }

        Node $$;

        if( lookahead(bitwiseor_token) ) {
            match(bitwiseor_token);
            $1 = NodeFactory.BinaryExpression(bitwiseor_token,$1,parseBitwiseXorExpression(mode));
            $$ = parseBitwiseOrExpressionPrime(mode,$1);
        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseBitwiseOrExpressionPrime" );
        }

        return $$;
    }

    /**
     * LogicalAndExpression	
     *     BitwiseOrExpression
     *     LogicalAndExpression && BitwiseOrExpression
     */

    final public Node parseLogicalAndExpression(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseLogicalAndExpression" );
        }

        Node $$,$1;

        $1 = parseBitwiseOrExpression(mode);
        $$ = parseLogicalAndExpressionPrime(mode,$1);
        
        if( debug ) {
            Debugger.trace( "finish parseLogicalAndExpression" );
        }

        return $$;
    }

    final public Node parseLogicalAndExpressionPrime(int mode, Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseLogicalAndExpressionPrime" );
        }

        Node $$;

        if( lookahead(logicaland_token) ) {
            match(logicaland_token);
            $1 = NodeFactory.BinaryExpression(logicaland_token,$1,parseBitwiseOrExpression(mode));
            $$ = parseLogicalAndExpressionPrime(mode,$1);
        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseLogicalAndExpressionPrime" );
        }

        return $$;
    }

    /**
     * LogicalXorExpression	
     *     LogicalAndExpression
     *     LogicalXorExpression ^^ LogicalAndExpression
     */

    final public Node parseLogicalXorExpression(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseLogicalXorExpression" );
        }

        Node $$,$1;

        $1 = parseLogicalAndExpression(mode);
        $$ = parseLogicalXorExpressionPrime(mode,$1);
        
        if( debug ) {
            Debugger.trace( "finish parseLogicalXorExpression" );
        }

        return $$;
    }

    final public Node parseLogicalXorExpressionPrime(int mode, Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseLogicalXorExpressionPrime" );
        }

        Node $$;

        if( lookahead(logicalxor_token) ) {
            match(logicalxor_token);
            $1 = NodeFactory.BinaryExpression(logicalxor_token,$1,parseLogicalAndExpression(mode));
            $$ = parseLogicalXorExpressionPrime(mode,$1);
        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseLogicalXorExpressionPrime" );
        }

        return $$;
    }

    /**
     * LogicalOrExpression	
     *     LogicalXorExpression
     *     LogicalOrExpression || LogicalXorExpression
     */

    final public Node parseLogicalOrExpression(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseLogicalOrExpression" );
        }

        Node $$,$1;

        $1 = parseLogicalXorExpression(mode);
        $$ = parseLogicalOrExpressionPrime(mode,$1);
        
        if( debug ) {
            Debugger.trace( "finish parseLogicalOrExpression" );
        }

        return $$;
    }

    final public Node parseLogicalOrExpressionPrime(int mode, Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseLogicalOrExpressionPrime" );
        }

        Node $$;

        if( lookahead(logicalor_token) ) {
            match(logicalor_token);
            $1 = NodeFactory.BinaryExpression(logicalor_token,$1,parseLogicalXorExpression(mode));
            $$ = parseLogicalOrExpressionPrime(mode,$1);
        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseLogicalOrExpressionPrime" );
        }

        return $$;
    }

    /**
     * ConditionalExpression	
     *     LogicalOrExpression
     *     LogicalOrExpression ? AssignmentExpression : AssignmentExpression
     */

    final public Node parseConditionalExpression(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseConditionalExpression" );
        }

        Node $$,$1;
        
        $1 = parseLogicalOrExpression(mode);

        if( lookahead(questionmark_token) ) {
            Node $2,$3;
            $2 = parseAssignmentExpression(mode);
            match(colon_token);
            $3 = parseAssignmentExpression(mode);
            $$ = NodeFactory.ConditionalExpression($1,$2,$3);
        } else {
            $$ = $1;
        }
    
        if( debug ) {
            Debugger.trace( "finish parseConditionalExpression" );
        }

        return $$;
    }

    /**
     * NonAssignmentExpression	
     *     LogicalOrExpression
     *     LogicalOrExpression ? NonAssignmentExpression : NonAssignmentExpression
     */

    final public Node parseNonAssignmentExpression(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseNonAssignmentExpression" );
        }

        Node $$,$1;
        
        $1 = parseLogicalOrExpression(mode);

        if( lookahead(questionmark_token) ) {
            Node $2,$3;
            $2 = parseNonAssignmentExpression(mode);
            match(colon_token);
            $3 = parseNonAssignmentExpression(mode);
            $$ = NodeFactory.ConditionalExpression($1,$2,$3);
        } else {
            $$ = $1;
        }


    
        if( debug ) {
            Debugger.trace( "finish parseNonAssignmentExpression" );
        }

        return $$;
    }

    static final void testNonAssignmentExpression() throws Exception {

        String[] input = { 
	        "",
	        "" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("NonAssignmentExpression",input,expected);
    }

    /**
     * AssignmentExpression	
     *     ConditionalExpression
     *     PostfixExpression AssignmentExpressionPrime
     * 	
     * AssignmentExpressionPrime	
     *     = AssignmentExpression AssignmentExpressionPrime
     *     CompoundAssignment AssignmentExpression
     */

    final public Node parseAssignmentExpression(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseAssignmentExpression" );
        }

        Node $$,$1;

        $1 = parseConditionalExpression(mode);
        $$ = parseAssignmentExpressionPrime(mode,$1);

        if( debug ) {
            Debugger.trace( "finish parseAssignmentExpression" );
        }

        return $$;
    }

    final public Node parseAssignmentExpressionPrime(int mode, Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseAssignmentExpressionPrime" );
        }

        Node $$;
        int  $2 = error_token;

        if( lookahead( assign_token ) ? (($2 = match( assign_token )) != error_token) :
            lookahead( multassign_token ) ? (($2 = match( multassign_token )) != error_token) :
            lookahead( divassign_token ) ? (($2 = match( divassign_token )) != error_token) :
            lookahead( modulusassign_token ) ? (($2 = match( modulusassign_token )) != error_token) :
            lookahead( plusassign_token ) ? (($2 = match( plusassign_token )) != error_token) :
            lookahead( minusassign_token ) ? (($2 = match( minusassign_token )) != error_token) :
            lookahead( leftshiftassign_token ) ? (($2 = match( leftshiftassign_token )) != error_token) :
            lookahead( rightshiftassign_token ) ? (($2 = match( rightshiftassign_token )) != error_token) :
            lookahead( unsignedrightshiftassign_token ) ? (($2 = match( unsignedrightshiftassign_token )) != error_token) :
            lookahead( bitwiseandassign_token ) ? (($2 = match( bitwiseandassign_token )) != error_token) :
            lookahead( bitwisexorassign_token )  ? (($2 = match( bitwisexorassign_token )) != error_token) :
            lookahead( bitwiseorassign_token )  ? (($2 = match( bitwiseorassign_token )) != error_token) : 
            lookahead( logicalandassign_token ) ? (($2 = match( bitwiseandassign_token )) != error_token) :
            lookahead( logicalxorassign_token )  ? (($2 = match( bitwisexorassign_token )) != error_token) :
            lookahead( logicalorassign_token )  ? (($2 = match( bitwiseorassign_token )) != error_token) : false ) {

            // ACTION: verify $1.isPostfixExpression();

            $$ = NodeFactory.AssignmentExpression($1,$2,
                     NodeFactory.CoersionExpression(parseAssignmentExpression(mode),
                            NodeFactory.ClassofExpression($1)));

        } else {

            $$ = $1;

        }

        if( debug ) {
            Debugger.trace( "finish parseAssignmentExpression" );
        }

        return $$;
    }
    
    static final void testAssignmentExpression() throws Exception {

        String[] input = { 
	        "x=10",
	        "x=y*=20", 
	        "x=y*=z>>>=20" 
        };

        String[] expected = { 
            "assign( assign_token, identifier( x ), literalnumber( 10 ))",
            "assign( assign_token, identifier( x ), assign( multassign_token, identifier( y ), literalnumber( 20 )))",
            "assign( assign_token, identifier( x ), assign( multassign_token, identifier( y ), assign( unsignedrightshiftassign_token, identifier( z ), literalnumber( 20 ))))"
        };

        testParser("AssignmentExpression",input,expected);
    }

    /**
     * ListExpression	
     *     AssignmentExpression
     *     ListExpression , AssignmentExpression
     */

    final public Node parseListExpression(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseListExpression" );
        }

        Node $$, $1;
        
        $1 = parseAssignmentExpression(mode);
        $$ = parseListExpressionPrime(mode,$1);

        if( debug ) {
            Debugger.trace( "finish parseListExpression" );
        }

        return $$;
    }

    private Node parseListExpressionPrime(int mode, Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseListExpressionPrime" );
        }

        Node $$;

        // , AssignmentExpression ListExpressionPrime

        if( lookahead( comma_token ) ) {

            match( comma_token );

            Node $2;
        
            $2 = parseAssignmentExpression(mode);
            $$ = parseListExpressionPrime(mode,NodeFactory.List($1,$2));

        // <empty>

        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseListExpression'" );
        }

        return $$;
    }

    static final void testListExpression() throws Exception {

        String[] input = { 
	        "x,1,'a'",
	        "z" 
        };

        String[] expected = { 
            "list( list( identifier( x ), literalnumber( 1 ) ), literalstring( a ) )",
            "identifier( z )" 
        };

        testParser("ListExpression",input,expected);
    }

    /**
     * TypeExpression
     *     NonassignmentExpression
     */

    final public Node parseTypeExpression(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseTypeExpression" );
        }

        Node $$ = parseNonAssignmentExpression(mode);
    
        if( debug ) {
            Debugger.trace( "finish parseTypeExpression" );
        }

        return $$;
    }

    static final void testTypeExpression() throws Exception {

        String[] input = { 
	        "Object",
	        "Widget" 
        };

        String[] expected = { 
            "identifier( Object )",
            "identifier( Widget )" 
        };

        testParser("TypeExpression",input,expected);
    }

    /**
     * TopStatement	
     *     Statement
     *     LanguageDeclaration NoninsertableSemicolon
     *     PackageDefinition
     */

    public final Node parseTopStatement(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseTopStatement" );
        }

        Node $$ = null;
    
        try {
		
		if( lookahead(use_token) ) {
            $$ = parseLanguageDeclaration();
            matchNoninsertableSemicolon(mode);
        } else if( lookahead(package_token) ) {
            $$ = parsePackageDefinition();
        } else {
            $$ = parseStatement(mode);
        }

		} catch ( Exception x ) {
		    // Do nothing. We are simply recovering from an error in the
			// current statement.
		}

        if( debug ) {
            Debugger.trace( "finish parseTopStatement" );
        }

        return $$;
    }

    static final void testTopStatement() throws Exception {

        String[] input = { 
	        "var x;",
	        "var y;" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("TopStatement",input,expected);
    }

    /**
     * Statement	
     *     EmptyStatement
     *     IfStatement
     *     SwitchStatement
     *     DoStatement Semicolon
     *     WhileStatement
     *     ForStatement
     *     WithStatement
     *     ContinueStatement Semicolon
     *     BreakStatement Semicolon
     *     ReturnStatement Semicolon
     *     ThrowStatement Semicolon
     *     TryStatement
     *     UseStatement Semicolon
     *     IncludeStatement Semicolon
     *     AnnotatedDefinition
     *     AnnotatedBlock
     *     ExpressionStatement Semicolon
     *     LabeledStatement
     */

    public final Node parseStatement(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseStatement" );
        }

        Node $$;
    
        if( lookahead(semicolon_token) ) {
            match(semicolon_token);
            $$ = NodeFactory.EmptyStatement();
        } else if( lookahead(if_token) ) {
            $$ = parseIfStatement(mode);
        } else if( lookahead(switch_token) ) {
            $$ = parseSwitchStatement();
        } else if( lookahead(do_token) ) {
            $$ = parseDoStatement();
            matchSemicolon(mode);
        } else if( lookahead(while_token) ) {
            $$ = parseWhileStatement(mode);
        } else if( lookahead(for_token) ) {
            $$ = parseForStatement(mode);
        } else if( lookahead(with_token) ) {
            $$ = parseWithStatement(mode);
        } else if( lookahead(continue_token) ) {
            $$ = parseContinueStatement();
            matchSemicolon(mode);
        } else if( lookahead(break_token) ) {
            $$ = parseBreakStatement();
            matchSemicolon(mode);
        } else if( lookahead(return_token) ) {
            $$ = parseReturnStatement();
            matchSemicolon(mode);
        } else if( lookahead(throw_token) ) {
            $$ = parseThrowStatement();
            matchSemicolon(mode);
        } else if( lookahead(try_token) ) {
            $$ = parseTryStatement();
        } else if( lookahead(use_token) ) {
            $$ = parseUseStatement();
            matchSemicolon(mode);
        } else if( lookahead(include_token) ) {
            $$ = parseIncludeStatement();
            matchSemicolon(mode);
        } else {
            $$ = parseAnnotatedOrLabeledOrExpressionStatement(mode);
        }

        if( debug ) {
            Debugger.trace( "finish parseStatement" );
        }

        return $$;
    }

    static final void testStatement() throws Exception {

        String[] input = { 
	        "var x;",
	        "var y;" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("Statement",input,expected);
    }

    /**
     * AnnotatedOrLabeledOrExpressionStatement	
     *     Attributes Block
     *     Attributes Definition
     *     Identifier : Statement
     *     [lookahead?{function, {, const }] ListExpressionallowIn ;
     */

    public final Node parseAnnotatedOrLabeledOrExpressionStatement(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseAnnotatedOrLabeledOrExpressionStatement" );
        }

        Node $$;

        if( lookahead(leftbrace_token) ) {
            $$ = NodeFactory.AnnotatedBlock(null,parseBlock());
        } else if( lookahead(import_token) || lookahead(export_token) ||
            lookahead(var_token) || lookahead(const_token) ||
            lookahead(function_token) || lookahead(class_token) ||
            lookahead(namespace_token) || lookahead(interface_token) ) {
            $$ = NodeFactory.AnnotatedDefinition(null,parseDefinition(mode));
        } else if( lookahead(abstract_token) || lookahead(final_token) ||
            lookahead(private_token) || lookahead(public_token) ||
            lookahead(static_token) ) {
            Node $1;
            $1 = parseAttributes();
            if( lookahead(leftbrace_token) ) {
                $$ = NodeFactory.AnnotatedBlock($1,parseBlock());
            } else {
                $$ = NodeFactory.AnnotatedDefinition($1,parseDefinition(mode));
            }
        } else {
            Node $1;
            $1 = parseListExpression(allowIn_mode);
            if( lookahead(colon_token) ) {
                // ACTION: verify that $1 is an IdentifierNode.
                match(colon_token);
                $$ = NodeFactory.LabeledStatement($1,parseStatement(mode));
            } else if( lookahead(leftbrace_token) ) {
                // ACTION: verify that $1 is an AttributeNode.
                $$ = NodeFactory.AnnotatedBlock($1,parseBlock());
            } else if( lookahead(semicolon_token) ) {
                match(semicolon_token);
                $$ = NodeFactory.ExpressionStatement($1);
            } else if( lookaheadSemicolon(mode) ) {
                $$ = NodeFactory.ExpressionStatement($1);
                matchSemicolon(mode);
            } else {
                // ACTION: verify that $1 is an AttributeNode.
                $1 = NodeFactory.AttributeList($1,parseAttributes());
                $$ = NodeFactory.AnnotatedDefinition($1,parseDefinition(mode));
            }
        }


        if( debug ) {
            Debugger.trace( "finish parseAnnotatedOrLabeledOrExpressionStatement" );
            Debugger.trace( "with $$ = " + $$ );
        }

        return $$;
    }

    /**
     * ExpressionStatement	
     *     [lookahead != { function,{,const }] ListExpression[allowIn]
     */

    public final Node parseExpressionStatement() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseExpressionStatement" );
        }

        Node $$;
    
        if( lookahead(function_token) || lookahead(leftbrace_token) ||
            lookahead(const_token) ) {
            $$ = null;  // Should not get here.
            if(debug) Debugger.trace("invalid lookahead for expression statement.");
        } else {
            $$ = NodeFactory.ExpressionStatement(parseListExpression(allowIn_mode));
        }

        if( debug ) {
            Debugger.trace( "finish parseExpressionStatement" );
        }

        return $$;
    }

    static final void testExpressionStatement() throws Exception {

        String[] input = { 
	        "x=1",
	        "x=y" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("ExpressionStatement",input,expected);
    }

    /**
     * AnnotatedBlock	
     *     Attributes Block
     */

    public final Node parseAnnotatedBlock() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseAnnotatedBlock" );
        }

        Node $$;

        $$ = NodeFactory.AnnotatedBlock(parseAttributes(),parseBlock());
    
        if( debug ) {
            Debugger.trace( "finish parseAnnotatedBlock" );
        }

        return $$;
    }

    static final void testAnnotatedBlock() throws Exception {

        String[] input = { 
	        "",
	        "" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("AnnotatedBlock",input,expected);
    }

    /**
     * Block	
     *     { TopStatements }
     */

    public final Node parseBlock() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseBlock" );
        }

        Node $$;
    
        match( leftbrace_token );
        $$ = parseTopStatements();
        match( rightbrace_token );

        if( debug ) {
            Debugger.trace( "finish parseBlock" );
        }

        return $$;
    }

    static final void testBlock() throws Exception {

        String[] input = { 
	        "{}",
	        "{}" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("Block",input,expected);
    }

    /**
     * TopStatements	
     *     «empty»
     *     TopStatementsPrefix TopStatement[abbrev]
     * 
     * TypeStatementsPrefix	
     *     TopStatement[full] TopStatementPrefixPrime
     * 	
     * TopStatementPrefixPrime	
     *     TopStatement[full] TopStatementPrefixPrime
     *     empty
     */

    public final Node parseTopStatements() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseTopStatements" );
        }

        Node $$;

        if( lookahead(rightbrace_token) || lookahead(eos_token) ) {

            $$ = null; 

        } else {
            
            StatementListNode $1;
            $1 = parseTopStatementsPrefix();
            if( !(lookahead(rightbrace_token) || lookahead(eos_token)) ) {
                Node $2;
                $2 = parseTopStatement(abbrev_mode);
                $$ = NodeFactory.StatementList($1,$2); 
            } else {
                $$ = NodeFactory.StatementList($1,null);
            }


        }

        if( debug ) {
            Debugger.trace( "finish parseTopStatements" );
        }

        return $$;
    }

    public final StatementListNode parseTopStatementsPrefix() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseTopStatementsPrefix" );
        }

        StatementListNode $$;
        Node $1;

        $1 = parseTopStatement(full_mode);
        $$ = parseTopStatementsPrefixPrime(NodeFactory.StatementList(null,$1));

        if( debug ) {
            Debugger.trace( "finish parseTopStatementsPrefix" );
        }

        return $$;
    }

    public final StatementListNode parseTopStatementsPrefixPrime(StatementListNode $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseTopStatementsPrefixPrime" );
        }

        StatementListNode $$;

        if( lookahead(rightbrace_token) || lookahead(eos_token) ) {

            $$ = NodeFactory.StatementList($1,null); 

        } else {
            
            Node $2;
            $2 = parseTopStatement(full_mode);
            $$ = parseTopStatementsPrefixPrime(NodeFactory.StatementList($1,$2));

        }

        if( debug ) {
            Debugger.trace( "finish parseTopStatementsPrefixPrime" );
        }

        return $$;
    }

    static final void testTopStatements() throws Exception {

        String[] input = { 
	        "class C;",
	        "var x; var y;",
            "x = 1;",
            "v1 var x;",
             
        };

        String[] expected = { 
            "classdeclaration( qualifiedidentifier( null, C ) )",
            "list( variabledefinition( var_token, variablebinding( typedvariable( qualifiedidentifier( null, x ), null ), null ) ), variabledefinition( var_token, variablebinding( typedvariable( qualifiedidentifier( null, y ), null ), null ) ) )",
            "expressionstatement( assign( assign_token, qualifiedidentifier( null, x ), coersionexpression( literalnumber( 1 ), qualifiedidentifier( null, x ) )) )",
            ""
        };

        testParser("TopStatements",input,expected);
    }

    /**
     * LabeledStatement	
     *     Identifier : Statement
     */

    public final Node parseLabeledStatement(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseLabeledStatement" );
        }

        Node $$,$1;
    
        $1 = parseIdentifier();
        match( colon_token );
        $$ = NodeFactory.LabeledStatement($1,parseStatement(mode));

        if( debug ) {
            Debugger.trace( "finish parseLabeledStatement" );
        }

        return $$;
    }

    /**
     * IfStatement	
     *     if ParenthesizedExpression Statement
     *     if ParenthesizedExpression Statement else Statement
     */

    public final Node parseIfStatement(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseIfStatement" );
        }

        Node $$,$1,$2,$3=null;

        match(if_token);
        $1 = parseParenthesizedListExpression();
        $2 = parseStatement(abbrevIfElse_mode);
        if( lookahead(else_token) ) {
            match(else_token);
            $3 = parseStatement(mode);
        }

        $$ = NodeFactory.IfStatement($1,$2,$3);

        if( debug ) {
            Debugger.trace( "finish parseIfStatement" );
        }

        return $$;
    }

    static final void testIfStatement() throws Exception {

        String[] input = { 
	        "if(x) t;",
	        "if(x) t; else e;",
	        "if(x) {a;b;c;} else {1;2;3;}",
        };

        String[] expected = { 
            "",
            "",
            "",
        };

        testParser("IfStatement",input,expected);
    }

    /**
     * SwitchStatement	
     *     switch ParenthesizedListExpression { CaseStatements }
     */

    public final Node parseSwitchStatement() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseSwitchStatement" );
        }

        Node $$,$1;
        StatementListNode $2;

        match(switch_token);
        $1 = parseParenthesizedListExpression();
        match(leftbrace_token);
        $2 = parseCaseStatements();
        match(rightbrace_token);
        
        $$ = NodeFactory.SwitchStatement($1,$2);

        if( debug ) {
            Debugger.trace( "finish parseSwitchStatement" );
        }

        return $$;
    }

    /**
     * CaseStatement	
     *     Statement
     *     CaseLabel
     */

    public final Node parseCaseStatement(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseCaseStatement" );
        }

        Node $$;

        if( lookahead(case_token) || lookahead(default_token) ) {
            $$ = parseCaseLabel();
        } else {
            $$ = parseStatement(mode);
        }

        if( debug ) {
            Debugger.trace( "finish parseCaseStatement" );
        }

        return $$;
    }

    /**
     * CaseLabel	
     *     case ListExpressionallowIn :
     *     default :
     */

    public final Node parseCaseLabel() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseCaseLabel" );
        }

        Node $$,$1;

        if( lookahead(case_token) ) {
            match(case_token);
            $$ = NodeFactory.CaseLabel(parseListExpression(allowIn_mode));
        } else if( lookahead(default_token) ) {
            match(default_token);
            $$ = NodeFactory.CaseLabel(null); // null argument means default case.
        } else {
            throw new Exception("expecting CaseLabel.");
        }
        match(colon_token);

        if( debug ) {
            Debugger.trace( "finish parseCaseLabel" );
        }

        return $$;
    }

    /**
     * CaseStatements
     *     «empty»
     *     CaseLabel
     *     CaseLabel CaseStatementsPrefix CaseStatementabbrev
     */

    public final StatementListNode parseCaseStatements() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseCaseStatements" );
        }

        StatementListNode $$;

        if( !lookahead(rightbrace_token) ) {
            Node $1;
            $1 = parseCaseLabel();
            if( !lookahead(rightbrace_token) ) {
                $$ = parseCaseStatementsPrefix(NodeFactory.StatementList(null,$1));
            } else {
                $$ = NodeFactory.StatementList(null,$1);
            }

        } else {
            $$ = null;
        }

        if( debug ) {
            Debugger.trace( "finish parseCaseStatements" );
        }

        return $$;
    }

    /**
     * CaseStatementsPrefix	
     *     «empty»
     *     CaseStatement[full] CaseStatementsPrefix
     */

    public final StatementListNode parseCaseStatementsPrefix(StatementListNode $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseCaseStatementsPrefix" );
        }

        StatementListNode $$=null;

        if( !lookahead(rightbrace_token) ) {
            $1 = NodeFactory.StatementList($1,parseCaseStatement(full_mode));
            while( !lookahead(rightbrace_token) ) {
                $1 = NodeFactory.StatementList($1,parseCaseStatement(full_mode));
            }
            $$ = $1;
        }


        if( debug ) {
            Debugger.trace( "finish parseCaseStatementsPrefix" );
        }

        return $$;
    }

    /**
     * DoStatement	
     *     do Statement[abbrev] while ParenthesizedListExpression
     */

    public final Node parseDoStatement() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseWhileStatement" );
        }

        Node $$,$1;

        match(do_token);    
        $1 = parseStatement(abbrevDoWhile_mode);
        match(while_token);    
        $$ = NodeFactory.DoStatement($1,parseParenthesizedListExpression());

        if( debug ) {
            Debugger.trace( "finish parseDoStatement" );
        }

        return $$;
    }

    /**
     * WhileStatement	
     *     while ParenthesizedListExpression Statement
     */

    public final Node parseWhileStatement(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseWhileStatement" );
        }

        Node $$;

        match(while_token);    
        $$ = NodeFactory.WhileStatement(parseParenthesizedListExpression(),parseStatement(mode));

        if( debug ) {
            Debugger.trace( "finish parseWhileStatement" );
        }

        return $$;
    }

    /**
     * ForStatement	
     *     for (  ; OptionalExpression ; OptionalExpression ) Statement
     *     for ( Attributes VariableDefinitionKind VariableBinding[noIn] in ListExpression[allowIn] ) Statement
     *     for ( Attributes VariableDefinitionKind VariableBindingList[noIn] ; OptionalExpression ; OptionalExpression ) Statement
     *     for ( AssignmentExpression[noIn] ListExpressionPrime[noIn] ; OptionalExpression ; OptionalExpression ) Statement
     *     for ( PostfixExpression in ListExpression[allowIn] ) Statement
     */

    public final Node parseForStatement(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseForStatement" );
        }

        Node $$;

        match(for_token);
        match(leftparen_token);

        if( lookahead(semicolon_token) ) {
            match(semicolon_token);
            Node $1,$2,$3;
            $1 = null;
            if( lookahead(semicolon_token) ) {
                $2 = null;
            } else {
                $2 = parseListExpression(allowIn_mode);
            }
            match(semicolon_token);
            if( lookahead(rightparen_token) ) {
                $3 = null;
            } else {
                $3 = parseListExpression(mode);
            }
            match(rightparen_token);
            $$ = NodeFactory.ForStatement($1,$2,$3,parseStatement(mode));
        } else if( lookahead(abstract_token) || lookahead(final_token) ||
            lookahead(private_token) || lookahead(public_token) ||
            lookahead(static_token) ||
            lookahead(const_token) || lookahead(var_token) ) {
            Node $1;
            $1 = parseAttributes();

            // [Copied below]

            if( lookahead(const_token) || lookahead(var_token) ) {
                int $2;
                Node $3;
                $2 = lookahead(const_token) ? match(const_token) :
                     lookahead(var_token) ? match(var_token) : var_token;
                $3 = parseVariableBinding(noIn_mode);
                if( lookahead(in_token) ) {
                    Node $4;
                    match(in_token);
                    $3 = NodeFactory.VariableDefinition($2,$3);
                    $3 = NodeFactory.AnnotatedDefinition($1,$3);
                    $4 = parseListExpression(allowIn_mode);
                    match(rightparen_token);
                    $$ = NodeFactory.ForInStatement($3,$4,parseStatement(mode));
                } else {
                    Node $4,$5;
                    $3 = parseVariableBindingListPrime(noIn_mode,$3);
                    $3 = NodeFactory.VariableDefinition($2,$3);
                    $3 = NodeFactory.AnnotatedDefinition($1,$3);
                    match(semicolon_token);
                    if( lookahead(semicolon_token) ) {
                        match(semicolon_token);
                        $4 = null;
                    } else {
                        $4 = parseListExpression(allowIn_mode);
                    }
                    match(semicolon_token);
                    if( lookahead(rightparen_token) ) {
                        $5 = null;
                    } else {
                        $5 = parseListExpression(allowIn_mode);
                    }
                    match(rightparen_token);
                    $$ = NodeFactory.ForStatement($3,$4,$5,parseStatement(mode));
                }
            } else {
                throw new Exception("expecting const or var token.");
            }
        } else {
            Node $1;
            $1 = parseAssignmentExpression(noIn_mode); // noIn

            // for ( AssignmentExpression[noIn] ListExpressionPrime[noIn] ; OptionalExpression ; OptionalExpression ) Statement
            if( lookahead(semicolon_token) ||
                lookahead(comma_token) ) {

                Node $2,$3;
                
                // Either way we should continue parsing as a ListExpression.
                $1 = parseListExpressionPrime(noIn_mode,$1);

                match(semicolon_token);
                if( lookahead(semicolon_token) ) {
                    $2 = null;
                } else {
                    $2 = parseListExpression(allowIn_mode);
                }
                match(semicolon_token);
                if( lookahead(rightparen_token) ) {
                    $3 = null;
                } else {
                    $3 = parseListExpression(allowIn_mode);
                }
                match(rightparen_token);
                $$ = NodeFactory.ForStatement($1,$2,$3,parseStatement(mode));

            // for ( PostfixExpression in ListExpression[allowIn] ) Statement
            } else if( lookahead(in_token) ) {
                match(in_token);
                Node $2;
                $2 = parseListExpression(allowIn_mode);
                match(rightparen_token);
                $$ = NodeFactory.ForInStatement($1,$2,parseStatement(mode));
            } else {

                // Otherwise assume that it is an AttributeExpression and continue
                // parsing as above (after seening an attribute keyword.

                // for ( Attributes VariableDefinitionKind VariableBinding[noIn] in ListExpression[allowIn] ) Statement
                // for ( Attributes VariableDefinitionKind VariableBindingList[noIn] ; OptionalExpression ; OptionalExpression ) Statement

                if( !true /* verify that $1 is an AttributeExpression */ ) {
                    throw new Exception("expecting an attribute expression.");
                }

                $1 = NodeFactory.AttributeList($1,parseAttributes());

                // [Copied from above.]

                if( lookahead(const_token) || lookahead(var_token) ) {
                    int $2;
                    Node $3;
                    $2 = lookahead(const_token) ? match(const_token) :
                         lookahead(var_token) ? match(var_token) : var_token;
                    $3 = parseVariableBinding(noIn_mode);
                    if( lookahead(in_token) ) {
                        Node $4;
                        match(in_token);
                        $3 = NodeFactory.VariableDefinition($2,$3);
                        $3 = NodeFactory.AnnotatedDefinition($1,$3);
                        $4 = parseListExpression(allowIn_mode);
                        match(rightparen_token);
                        $$ = NodeFactory.ForInStatement($3,$4,parseStatement(mode));
                    } else {
                        Node $4,$5;
                        $3 = parseVariableBindingListPrime(noIn_mode,$3);
                        $3 = NodeFactory.VariableDefinition($2,$3);
                        $3 = NodeFactory.AnnotatedDefinition($1,$3);
                        match(semicolon_token);
                        if( lookahead(semicolon_token) ) {
                            $4 = null;
                        } else {
                            $4 = parseListExpression(allowIn_mode);
                        }
                        match(semicolon_token);
                        if( lookahead(rightparen_token) ) {
                            $5 = null;
                        } else {
                            $5 = parseListExpression(allowIn_mode);
                        }
                        match(rightparen_token);
                        $$ = NodeFactory.ForStatement($3,$4,$5,parseStatement(mode));
                    }
                } else {
                    throw new Exception("expecting const or var token.");
                }
            }
        }

        return $$;
    }

    /**
     * WithStatement	
     *     with ParenthesizedListExpression Statement
     */

    public final Node parseWithStatement(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseWithStatement" );
        }

        Node $$;

        match(with_token);    
        $$ = NodeFactory.WithStatement(parseParenthesizedListExpression(),parseStatement(mode));

        if( debug ) {
            Debugger.trace( "finish parseWithStatement" );
        }

        return $$;
    }

    /**
     * ContinueStatement	
     *     continue
     *     continue [no line break] Identifier
     */

    public final Node parseContinueStatement() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseContinueStatement" );
        }

        Node $$,$1=null;

        match(continue_token);    
        if( !lookaheadSemicolon(full_mode) ) {
            $1 = parseIdentifier();
        }

        $$ = NodeFactory.ContinueStatement($1);

        if( debug ) {
            Debugger.trace( "finish parseContinueStatement" );
        }

        return $$;
    }

    /**
     * BreakStatement	
     *     break
     *     break [no line break] Identifier
     */

    public final Node parseBreakStatement() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseBreakStatement" );
        }

        Node $$,$1=null;

        match(break_token);    
        if( !lookaheadSemicolon(full_mode) ) {
            $1 = parseIdentifier();
        }

        $$ = NodeFactory.BreakStatement($1);

        if( debug ) {
            Debugger.trace( "finish parseBreakStatement" );
        }

        return $$;
    }

    /**
     * ReturnStatement	
     *     return
     *     return [no line break] ExpressionallowIn
     */

    public final Node parseReturnStatement() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseReturnStatement" );
        }

        Node $$,$1=null;

        match(return_token);
        
        // ACTION: check for VirtualSemicolon
            
        if( !lookaheadSemicolon(full_mode) ) {
            $1 = parseListExpression(allowIn_mode);
        }

        $$ = NodeFactory.ReturnStatement($1);

        if( debug ) {
            Debugger.trace( "finish parseReturnStatement" );
        }

        return $$;
    }

    static final void testReturnStatement() throws Exception {

        String[] input = { 
	        "return;",
	        "return true;" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("ReturnStatement",input,expected);
    }

    /**
     * ThrowStatement 	
     *     throw [no line break] ListExpression[allowIn]
     */

    public final Node parseThrowStatement() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseThrowStatement" );
        }

        Node $$,$1;

        match(throw_token);
        lookahead(empty_token);  // ACTION: fix newline() so that we don't 
                                 // have to force a lookahead to the next 
                                 // token before it works properly.
        if( newline() ) {
            throw new Exception("No newline allowed in this position.");
        }
        $$ = NodeFactory.ThrowStatement(parseListExpression(allowIn_mode));

        if( debug ) {
            Debugger.trace( "finish parseThrowStatement" );
        }

        return $$;
    }

    /**
     * TryStatement	
     *     try AnnotatedBlock CatchClauses
     *     try AnnotatedBlock FinallyClause
     *     try AnnotatedBlock CatchClauses FinallyClause
     */

    public final Node parseTryStatement() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseTryStatement" );
        }

        Node $$,$1;

        match(try_token);
        $1 = parseAnnotatedBlock();
        if( lookahead(catch_token) ) {
            StatementListNode $2 = parseCatchClauses();
            if( lookahead(finally_token) ) {
                $$ = NodeFactory.TryStatement($1,$2,parseFinallyClause());
            } else {
                $$ = NodeFactory.TryStatement($1,$2,null);
            }
        } else if( lookahead(finally_token) ) {
            $$ = NodeFactory.TryStatement($1,null,parseFinallyClause());
        } else {
            throw new Exception("expecting catch or finally clause.");
        }

        if( debug ) {
            Debugger.trace( "finish parseTryStatement" );
        }

        return $$;
    }

    /**
     * CatchClauses	
     *     CatchClause
     *     CatchClauses CatchClause
     */

    public final StatementListNode parseCatchClauses() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseCatchClauses" );
        }

        StatementListNode $$;

        $$ = NodeFactory.StatementList(null,parseCatchClause());
        while( lookahead(catch_token) ) {
            $$ = NodeFactory.StatementList($$,parseCatchClause());
        }

        if( debug ) {
            Debugger.trace( "finish parseCatchClauses" );
        }

        return $$;
    }

    /**
     * CatchClause	
     *     catch ( Parameter ) AnnotatedBlock
     */

    public final Node parseCatchClause() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseCatchClause" );
        }

        Node $$,$1;

        match(catch_token);
        match(leftparen_token);
        $1 = parseParameter();
        match(rightparen_token);
               
        $$ = NodeFactory.CatchClause($1,parseAnnotatedBlock());

        if( debug ) {
            Debugger.trace( "finish parseCatchClause" );
        }

        return $$;
    }

    /**
     * FinallyClause	
     *     finally AnnotatedBlock
     */

    public final Node parseFinallyClause() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseFinallyClause" );
        }

        Node $$;

        match(finally_token);
        
        // No line break.

        $$ = NodeFactory.FinallyClause(parseAnnotatedBlock());

        if( debug ) {
            Debugger.trace( "finish parseFinallyClause" );
        }

        return $$;
    }

    /**
     * IncludeStatement	
     *     include [no line break] String
     */

    public final Node parseIncludeStatement() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseIncludeStatement" );
        }

        Node $$,$1;

        match(include_token);
        
        // No line break.

        $1 = NodeFactory.LiteralString(scanner.getTokenText(match(stringliteral_token)));
        $$ = NodeFactory.IncludeStatement($1);

        if( debug ) {
            Debugger.trace( "finish parseIncludeStatement" );
        }

        return $$;
    }

    static final void testIncludeStatement() throws Exception {

        String[] input = { 
	        "",
        };

        String[] expected = { 
            "",
        };

        testParser("IncludeStatement",input,expected);
    }

    /**
     * UseStatement	
     *     use [no line break] namespace NonAssignmentExpressionList
     */

    public final Node parseUseStatement() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseUseStatement" );
        }

        Node $$;

        match(use_token);
        match(namespace_token);
        $$ = NodeFactory.UseStatement(parseNonAssignmentExpression(allowIn_mode));

        if( debug ) {
            Debugger.trace( "finish parseUseStatement" );
        }

        return $$;
    }

    static final void testUseStatement() throws Exception {

        String[] input = { 
	        "use namespace P1",
	        "use namespace P1.P2.P3", 
	        "use namespace P1.P2.P3,P1,P4", 
        };

        String[] expected = { 
            "usestatement( qualifiedidentifier( null, P1 ) )",
            "usestatement( memberexpression( memberexpression( qualifiedidentifier( null, P1 ), qualifiedidentifier( null, P2 ) ), qualifiedidentifier( null, P3 ) ) )",
            "usestatement( list( list( memberexpression( memberexpression( qualifiedidentifier( null, P1 ), qualifiedidentifier( null, P2 ) ), qualifiedidentifier( null, P3 ) ), qualifiedidentifier( null, P1 ) ), qualifiedidentifier( null, P4 ) ) )", 
        };

        testParser("UseStatement",input,expected);
    }

    /**
     * NonAssignmentExpressionList	
     *     NonAssignmentExpression[allowIn] NonAssignmentExpressionListPrime
     *     
     * NonAssignmentExpressionListPrime	
     *     , NonAssignmentExpression[allowIn] NonAssignmentExpressionListPrime
     *     empty
     */

    final public Node parseNonAssignmentExpressionList() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseNonAssignmentExpressionList" );
        }

        Node $$, $1;
        
        $1 = parseNonAssignmentExpression(allowIn_mode);
        $$ = parseNonAssignmentExpressionListPrime($1);

        if( debug ) {
            Debugger.trace( "finish parseNonAssignmentExpressionList" );
        }

        return $$;
    }

    private Node parseNonAssignmentExpressionListPrime( Node $1 ) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseNonAssignmentExpressionListPrime" );
        }

        Node $$;

        if( lookahead( comma_token ) ) {

            match( comma_token );

            Node $2;
        
            $2 = parseNonAssignmentExpression(allowIn_mode);
            $$ = parseNonAssignmentExpressionListPrime(NodeFactory.List($1,$2));

        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseNonAssignmentExpressionListPrime'" );
        }

        return $$;
    }

    static final void testNonAssignmentExpressionList() throws Exception {

        String[] input = { 
	        "x,1,2",
	        "z" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("NonAssignmentExpressionList",input,expected);
    }

    /**
     * AnnotatedDefinition	
     *     Attributes Definition
     */

    public final AnnotatedDefinitionNode parseAnnotatedDefinition(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseAnnotatedDefinition" );
        }

        AnnotatedDefinitionNode $$;
        Node $1;

        $1 = parseAttributes();
        $$ = NodeFactory.AnnotatedDefinition($1,parseDefinition(mode));
            
        if( debug ) {
            Debugger.trace( "finish parseAnnotatedDefinition" );
        }

        return $$;
    }

    static final void testAnnotatedDefinition() throws Exception {

        String[] input = { 
	        "private var x",
	        "v2 const y",
	        "constructor function c() {}", 
        };

        String[] expected = { 
            "",
            "",
            "",
        };

        testParser("AnnotatedDefinition",input,expected);
    }

    /**
     * Attributes	
     *     «empty»
     *     Attribute [no line break] Attributes
     */

    public final Node parseAttributes() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseAttributes" );
        }

        Node $$;

        if( lookahead(leftbrace_token) || 
            lookahead(export_token) || 
            lookahead(import_token) || 
            lookahead(const_token) ||
            lookahead(var_token) ||
            lookahead(function_token) || 
            lookahead(class_token) ||
            lookahead(interface_token) || 
            lookahead(namespace_token) ) {

            $$ = null;

        } else {

            Node $1,$2;

            $1 = parseAttribute();

            if( newline() ) {
                scanner.error(scanner.syntax_error,"No line break in attributes list.");
			    throw new Exception();
            }

            $2 = parseAttributes();
            $$ = NodeFactory.AttributeList($1,$2);

        }

        if( debug ) {
            Debugger.trace( "finish parseAttributes" );
        }

        return $$;
    }

    static final void testAttributes() throws Exception {

        String[] input = { 
	        "private export",
	        "private public export" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("Attributes",input,expected);
    }

    /**
     * Attribute	
     *     AttributeExpression
     *     abstract
     *     final
     *     private
     *     public
     *     static
     */

    public final Node parseAttribute() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseAttribute" );
        }

        Node $$;
    
        if( lookahead(abstract_token) ) {
            match(abstract_token);
            $$ = NodeFactory.Identifier("abstract");
        } else if( lookahead(final_token) ) {
            match(final_token);
            $$ = NodeFactory.Identifier("final");
        } else if ( lookahead(private_token) ) {
            match(private_token);
            $$ = NodeFactory.Identifier("private");
        } else if ( lookahead(public_token) ) {
            match(public_token);
            $$ = NodeFactory.Identifier("public");
        } else if ( lookahead(static_token) ) {
            match(static_token);
            $$ = NodeFactory.Identifier("static");
        } else if ( lookahead(true_token) ) {
            match(true_token);
            $$ = NodeFactory.LiteralBoolean(true);
        } else if ( lookahead(false_token) ) {
            match(false_token);
            $$ = NodeFactory.LiteralBoolean(false);
        } else {
            $$ = parseAttributeExpression();
        }

        if( debug ) {
            Debugger.trace( "finish parseAttribute" );
        }

        return $$;
    }

    static final void testAttribute() throws Exception {

        String[] input = { 
	        "final",
	        "package",
	        "private",
	        "public",
	        "static",
	        "volatile",
	        "pascal" 
        };

        String[] expected = { 
            "",
            "",
            "",
            "",
            "",
            "",
            "" 
        };

        testParser("Attribute",input,expected);
    }

    /**
     * Definition	
     *     ImportDefinition Semicolon
     *     ExportDefinition Semicolon
     *     VariableDefinition Semicolon
     *     FunctionDefinition
     *     ClassDefinition
     *     NamespaceDefinition Semicolon
     *     InterfaceDefinition
     */

    public final Node parseDefinition(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseDefinition" );
        }

        Node $$ = null;
    
        if( lookahead(import_token) ) {
            $$ = parseImportDefinition();
            matchSemicolon(mode);
        } else if( lookahead(export_token) ) {
            $$ = parseExportDefinition();
            matchSemicolon(mode);
        } else if( lookahead(const_token) || lookahead(var_token) ) {
            $$ = parseVariableDefinition();
            matchSemicolon(mode);
        } else if( lookahead(function_token) ) {
            $$ = parseFunctionDefinition(mode);
        } else if( lookahead(class_token) ) {
            $$ = parseClassDefinition(mode);
        } else if( lookahead(interface_token) ) {
            $$ = parseInterfaceDefinition(mode);
        } else if( lookahead(namespace_token) ) {
            $$ = parseNamespaceDefinition();
            matchSemicolon(mode);
        }

        if( debug ) {
            Debugger.trace( "finish parseDefinition" );
        }

        return $$;
    }

    static final void testDefinition() throws Exception {

        String[] input = { 
	        "var x;",
	        "class C;" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("TopStatement",input,expected);
    }

    /**
     * ImportDefinition	
     *     import ImportBinding
     *     import ImportBinding : NonassignmentExpressionList
     */

    final public Node parseImportDefinition() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseImportDefinition" );
        }

        Node $$,$1,$2=null;
        
        match(import_token);
        $1 = parseImportBinding();
        if( lookahead(colon_token) ) {
            $2 = parseNonAssignmentExpressionList();
        }

        $$ = NodeFactory.ImportDefinition($1,$2);
    
        if( debug ) {
            Debugger.trace( "finish parseImportDefinition" );
        }

        return $$;
    }

    static final void testImportDefinition() throws Exception {

        String[] input = { 
	        "import x",
	        "import f=g",
            "import f=g,h" 
        };

        String[] expected = { 
            "",
            "",
            "",
        };

        testParser("ImportDefinition",input,expected);
    }

    /**
     * ImportBinding	
     *     Identifier PackageNamePrime
     *     Identifier = ImportItem
     *     String
     */

    final public Node parseImportBinding() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseImportBinding" );
        }

        Node $$,$1,$2;
        
        if( lookahead(stringliteral_token) ) {
            $$ = NodeFactory.LiteralString(scanner.getTokenText(match(stringliteral_token)));
        } else {
            $1 = parseIdentifier();
            if( lookahead(dot_token) ) {
                $$ = parsePackageNamePrime($1);
            } else if( lookahead(assign_token) ) {
                match(assign_token);
                $$ = NodeFactory.ImportBinding($1,parseImportItem());
            } else {
                throw new Exception("expecting dot or assignment after identifier.");
            }
        }

        if( debug ) {
            Debugger.trace( "finish parseImportBinding" );
        }

        return $$;
    }

    /**
     * ImportItem	
     *     String
     *     PackageName
     */

    final public Node parseImportItem() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseImportItem" );
        }

        Node $$;
        
        if( lookahead(stringliteral_token) ) {
            $$ = NodeFactory.LiteralString(scanner.getTokenText(match(stringliteral_token)));
        } else {
            $$ = parsePackageName();
        }

        if( debug ) {
            Debugger.trace( "finish parseImportItem" );
        }

        return $$;
    }

    /**
     * ExportDefinition	
     *     export ExportBindingList
     */

    final public Node parseExportDefinition() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseExportDefinition" );
        }

        Node $$,$1;
        
        match(export_token);
        $1 = parseExportBindingList();
        $$ = NodeFactory.ExportDefinition($1);

    
        if( debug ) {
            Debugger.trace( "finish parseExportDefinition" );
        }

        return $$;
    }

    static final void testExportDefinition() throws Exception {

        String[] input = { 
	        "export x",
	        "export f=g",
            "export f=g,h" 
        };

        String[] expected = { 
            "",
            "",
            "",
        };

        testParser("ExportDefinition",input,expected);
    }

    /**
     * ExportBindingList	
     *     ExportBinding ExportBindingListPrime
     * 	
     * ExportBindingListPrime	
     *     , ExportBinding ExportBindingListPrime
     *     empty
     */

    final public Node parseExportBindingList() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseExportBindingList" );
        }

        Node $$,$1;
        
        $1 = parseExportBinding();
        $$ = parseExportBindingListPrime(NodeFactory.List(null,$1));

        if( debug ) {
            Debugger.trace( "finish parseExportBindingList" );
        }

        return $$;
    }

    private Node parseExportBindingListPrime( Node $1 ) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseExportBindingListPrime" );
        }

        Node $$;

        if( lookahead( comma_token ) ) {

            match( comma_token );

            Node $2;
        
            $2 = parseExportBinding();
            $$ = parseExportBindingListPrime(NodeFactory.List($1,$2));

        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseExportBindingListPrime'" );
        }

        return $$;
    }

    static final void testExportBindingList() throws Exception {

        String[] input = { 
	        "x,y,z",
	        "z" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("ExportBindingList",input,expected);
    }

    /**
     * ExportBinding	
     *     FunctionName
     *     FunctionName = FunctionName
     */

    final public Node parseExportBinding() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseExportBinding" );
        }

        Node $$,$1,$2;
        
        $1 = parseFunctionName();
        if( lookahead(assign_token) ) {
            match(assign_token);
            $2 = parseFunctionName();
        } else {
            $2 = null;
        }

        $$ = NodeFactory.ExportBinding($1,$2);

    
        if( debug ) {
            Debugger.trace( "finish parseExportBinding" );
        }

        return $$;
    }

    static final void testExportBinding() throws Exception {

        String[] input = { 
	        "",
	        "" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("ExportBinding",input,expected);
    }

    /**
     * VariableDefinition	
     *     VariableDefinitionKind VariableBindingList[allowIn]
     */

    final public Node parseVariableDefinition() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseVariableDefinition" );
        }

        int  $1;
        Node $$,$2;
        
        // The following logic goes something like this: If it is a
        // const_token, then $1 is a const_token. If it is a var_token
        // then $1 is var_token. If it is anything else then $1 is
        // the default (var_token).

        $1 = lookahead(const_token) ? match(const_token) :
             lookahead(var_token) ? match(var_token) : var_token;

        $2 = parseVariableBindingList(allowIn_mode);
        $$ = NodeFactory.VariableDefinition($1,$2);
    
        if( debug ) {
            Debugger.trace( "finish parseVariableDefinition" );
        }

        return $$;
    }

    static final void testVariableDefinition() throws Exception {

        String[] input = { 
	        "var x=10",
	        "const pi:Number=3.1415",
            "var n:Number,o:Object,b:Boolean=false" 
        };

        String[] expected = { 
            "variabledefinition( var_token, variablebinding( typedvariable( qualifiedidentifier( null, x ), null ), literalnumber( 10 ) ) )",
            "variabledefinition( const_token, variablebinding( typedvariable( qualifiedidentifier( null, pi ), qualifiedidentifier( null, Number ) ), literalnumber( 3.1415 ) ) )",
            "variabledefinition( var_token, list( list( variablebinding( typedvariable( qualifiedidentifier( null, n ), qualifiedidentifier( null, Number ) ), null ), variablebinding( typedvariable( qualifiedidentifier( null, o ), qualifiedidentifier( null, Object ) ), null ) ), variablebinding( typedvariable( qualifiedidentifier( null, b ), qualifiedidentifier( null, Boolean ) ), literalboolean( false ) ) ) )"
        };

        testParser("VariableDefinition",input,expected);
    }

    /**
     * VariableBindingList	
     *     VariableBinding VariableBindingListPrime
     * 	
     * VariableBindingListPrime	
     *     , VariableBinding VariableBindingListPrime
     *     empty
     */

    final public Node parseVariableBindingList(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseVariableBindingList" );
        }

        Node $$,$1;
        
        $1 = parseVariableBinding(mode);
        $$ = parseVariableBindingListPrime(mode,NodeFactory.List(null,$1));

        if( debug ) {
            Debugger.trace( "finish parseVariableBindingList" );
        }

        return $$;
    }

    private Node parseVariableBindingListPrime(int mode, Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseVariableBindingListPrime" );
        }

        Node $$;

        if( lookahead( comma_token ) ) {

            match( comma_token );

            Node $2;
        
            $2 = parseVariableBinding(mode);
            $$ = parseVariableBindingListPrime(mode,NodeFactory.List($1,$2));

        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseVariableBindingListPrime'" );
        }

        return $$;
    }

    static final void testVariableBindingList() throws Exception {

        String[] input = { 
	        "x,y,z",
	        "z" 
        };

        String[] expected = { 
            "list( list( variablebinding( typedvariable( identifier( x ), null ), null ), variablebinding( typedvariable( identifier( y ), null ), null ) ), variablebinding( typedvariable( identifier( z ), null ), null ) )",
            "variablebinding( typedvariable( identifier( z ), null ), null )" 
        };

        testParser("VariableBindingList",input,expected);
    }

    /**
     * VariableBinding	
     *     TypedVariable
     *     TypedVariable = AssignmentExpression
     *     TypedVariable = MultipleAttributes
     */

    final public Node parseVariableBinding(int mode, boolean[] hasInitializer) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseVariableBinding" );
        }

        Node $$,$1,$2;
        
        $1 = parseTypedVariable(mode);
        if( lookahead(assign_token) ) {
            match(assign_token);
            if( lookahead(abstract_token) || lookahead(final_token) ||
                lookahead(private_token) || lookahead(public_token) ||
                lookahead(static_token) ) {
                $2 = parseMultipleAttributes();
            } else {
                $2 = parseAssignmentExpression(mode);
                if( lookahead(semicolon_token) ) {
                    // do nothing.
                } else {
                    $2 = parseMultipleAttributesPrime($2);
                }
            }
        } else {
            $2 = null;
        }

        $$ = NodeFactory.VariableBinding($1,$2);
    
        if( debug ) {
            Debugger.trace( "finish parseVariableBinding" );
        }

        return $$;
    }

    final public Node parseVariableBinding(int mode) throws Exception {
        boolean hasValue[] = {false};
        return parseVariableBinding(mode,hasValue);
    }

    static final void testVariableBinding() throws Exception {

        String[] input = { 
	        "",
	        "" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("VariableBinding",input,expected);
    }

    /**
     * MultipleAttributes	
     *     	Attribute [no line break] Attribute
     *     	MulitpleAttributes [no line break] Attribute
     */

    final public Node parseMultipleAttributes() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseMultipleAttributes" );
        }

        Node $$,$1,$2;
        
        $1 = parseAttribute();
        $$ = parseMultipleAttributesPrime($1);

        if( debug ) {
            Debugger.trace( "finish parseMultipleAttributes" );
        }

        return $$;
    }

    final public Node parseMultipleAttributesPrime(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseMultipleAttributes" );
        }

        Node $$;
        
        if( newline() ) {
            scanner.error(scanner.syntax_error,"No line break in multiple attributes definition.");
			throw new Exception();
        }
            
        $1 = NodeFactory.List($1,parseAttribute());
        
        while( !lookahead(semicolon_token) ) {

            if( newline() ) {
                throw new Exception("No line break in multiple attributes definition.");
            }
            
            $1 = NodeFactory.List($1,parseAttribute());
        
        }

        $$ = $1;

        if( debug ) {
            Debugger.trace( "finish parseMultipleAttributes" );
        }

        return $$;
    }

    /**
     * TypedVariable	
     *     	Identifier
     *     	Identifier : TypeExpression
     */

    final public Node parseTypedVariable(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseTypedVariable" );
        }

        Node $$,$1,$2;
        
        $1 = parseIdentifier();
        if( lookahead(colon_token) ) {
            match(colon_token);
            $2 = parseTypeExpression(mode);
        } else {
            $2 = null;
        }

        $$ = NodeFactory.TypedVariable($1,$2);
    
        if( debug ) {
            Debugger.trace( "finish parseTypedVariable" );
        }

        return $$;
    }

    static final void testTypedVariable() throws Exception {

        String[] input = { 
	        "a::b::c::x=10",
	        "y" 
        };

        String[] expected = { 
            "typedvariable( qualifiedidentifier( list( list( list( null, identifier( a ) ), identifier( b ) ), identifier( c ) ), x ), null )",
            "typedvariable( qualifiedidentifier( null, y ), null )" 
        };

        testParser("TypedVariable",input,expected);
    }

    /**
     * FunctionDefinition	
     *     FunctionDeclaration Block
     *     FunctionDeclaration Semicolon
     */

    final public Node parseFunctionDefinition(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseFunctionDefinition" );
        }

        Node $$,$1;

        $1 = parseFunctionDeclaration();
        if( lookahead(leftbrace_token) ) {
            Node $2;
            $2 = parseBlock();
            $$ = NodeFactory.FunctionDefinition($1,$2);
        } else {
            matchSemicolon(mode);
            $$ = $1;
        }
         
        if( debug ) {
            Debugger.trace( "finish parseFunctionDefinition" );
        }

        return $$;
    }

    static final void testFunctionDefinition() throws Exception {

        String[] input = { 
	        "function f();",
	        "function f(x,y) {var z;}", 
        };

        String[] expected = { 
            "",
            "",
            "",
        };

        testParser("FunctionDefinition",input,expected);
    }

    /**
     * FunctionDeclaration	
     *     function FunctionName FunctionSignature
     */

    final public Node parseFunctionDeclaration() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseFunctionDeclaration" );
        }

        Node $$,$1,$2;
    
        match(function_token);
        $1 = parseFunctionName();
        $2 = parseFunctionSignature();
        $$ = NodeFactory.FunctionDeclaration($1,$2);

        if( debug ) {
            Debugger.trace( "finish parseFunctionDeclaration" );
        }

        return $$;
    }

    static final void testFunctionDeclaration() throws Exception {

        String[] input = { 
	        "",
	        "" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("FunctionDeclaration",input,expected);
    }

    /**
     * FunctionName	
     *     Identifier
     *     get [no line break] Identifier
     *     set [no line break] Identifier
     */

    final public Node parseFunctionName() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseFunctionName" );
        }

        Node $$;

        if( lookahead( get_token ) ) {
            match( get_token );
            $$ = NodeFactory.FunctionName(get_token,parseIdentifier());
        } else if( lookahead( set_token ) ) {
            match( set_token );
            $$ = NodeFactory.FunctionName(set_token,parseIdentifier());
        } else {
            $$ = NodeFactory.FunctionName(empty_token,parseIdentifier());
        }

        if( debug ) {
            Debugger.trace( "finish parseFunctionName" );
        }
        return $$;
    }

    static final void testFunctionName() throws Exception {

        Debugger.trace( "begin testFunctionName" );

        String[] input = { 
            "x", 
            "get x", 
            "set x" 
        };

        String[] expected = { 
            "", 
            "", 
            "" 
        };

        testParser("FunctionName",input,expected);
    }

    /**
     * FunctionSignature	
     *     ParameterSignature ResultSignature
     * 
     * ParameterSignature	
     *     ( Parameters )
     */

    public final Node parseFunctionSignature() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseFunctionSignature" );
        }

        Node $$,$1,$2;
    
        // inlined: parseParameterSignature

        match( leftparen_token ); 
        $1 = parseParameters();
        match( rightparen_token );
        $2 = parseResultSignature();

        $$ = new FunctionSignatureNode($1,$2);
         
        if( debug ) {
            Debugger.trace( "finish parseFunctionSignature" );
        }

        return $$;
    }

    static final void testFunctionSignature() throws Exception {

        String[] input = { 
	        "(u,v:String='default', | 'a' x:String='default', 'b' y:Number,...z:Object) : Boolean",
	        "(u,v:String='default', 'a' x:String='default', | 'b' y:Number,...z:Object) : Boolean",
        };

        String[] expected = { 
            "", 
            "", 
        };

        testParser("FunctionSignature",input,expected);
    }

    /**
     * Parameters	
     *     	«empty»
     *     	AllParameters
     */

    final public Node parseParameters() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseParameters" );
        }

        Node $$,$1,$2;
        
        if( lookahead(rightparen_token) ) {
            $$ = null;
        } else {
            $$ = parseAllParameters();
        }

        if( debug ) {
            Debugger.trace( "finish parseParameters" );
        }

        return $$;
    }

    static final void testParameters() throws Exception {

        String[] input = { 
	        "",
            "",
        };

        String[] expected = { 
            "",
            "",
        };

        testParser("Parameters",input,expected);
    }

    /**
     * AllParameters	
     *     Parameter
     *     Parameter , AllParameters
     *     Parameter OptionalParameterPrime
     *     Parameter OptionalParameterPrime , OptionalNamedRestParameters
     *     | NamedRestParameters
     *     RestParameter
     *     RestParameter , | NamedParameters
     */

    final public Node parseAllParameters() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseAllParameters" );
        }

        Node $$;
        
        if( lookahead(tripledot_token) ) {
            Node $1;
            $1 = NodeFactory.List(null,parseRestParameter());
            if( lookahead(comma_token) ) {
                match(comma_token);
                match(bitwiseor_token);
                $$ = NodeFactory.List($1,parseNamedParameters());
            } else {
                $$ = $1;
            }
        } else if( lookahead(bitwiseor_token) ) {
            match(bitwiseor_token);
            $$ = parseNamedRestParameters();
        } else {
            Node $1;
            $1 = NodeFactory.List(null,parseParameter());
            if( lookahead(comma_token) ) {
                match(comma_token);
                $$ = NodeFactory.List($1,parseAllParameters());
            } else if( lookahead(assign_token) ) {
                $1 = parseOptionalParameterPrime($1);
                if( lookahead(comma_token) ) {
                    match(comma_token);
                    $$ = NodeFactory.List($1,parseOptionalNamedRestParameters());
                } else {
                    $$ = $1;
                }
            } else {
                $$ = $1;
            }
        }
    
        if( debug ) {
            Debugger.trace( "finish parseAllParameters" );
        }

        return $$;
    }

    /**
     * OptionalNamedRestParameters	
     *     OptionalParameter
     *     OptionalParameter , OptionalNamedRestParameters
     *     | NamedRestParameters
     *     RestParameter
     *     RestParameter , | NamedParameters
     */

    final public Node parseOptionalNamedRestParameters() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseOptionalNamedRestParameters" );
        }

        Node $$;
        
        if( lookahead(tripledot_token) ) {
            Node $1;
            $1 = parseRestParameter();
            if( lookahead(comma_token) ) {
                match(comma_token);
                match(bitwiseor_token);
                $$ = NodeFactory.List($1,parseNamedParameters());
            } else {
                $$ = $1;
            }
        } else if( lookahead(bitwiseor_token) ) {
            match(bitwiseor_token);
            $$ = parseNamedRestParameters();
        } else {
            Node $1;
            $1 = parseOptionalParameter();
            if( lookahead(comma_token) ) {
                match(comma_token);
                $$ = NodeFactory.List($1,parseOptionalNamedRestParameters());
            } else {
                $$ = $1;
            }
        }
    
        if( debug ) {
            Debugger.trace( "finish parseOptionalNamedRestParameters" );
        }

        return $$;
    }

    /**
     * NamedRestParameters	
     *     NamedParameter
     *     NamedParameter , NamedRestParameters
     *     RestParameter
     */

    final public Node parseNamedRestParameters() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseNamedRestParameters" );
        }

        Node $$;
        
        if( lookahead(tripledot_token) ) {
            $$ = parseRestParameter();
        } else {
            Node $1;
            $1 = parseNamedParameter();
            if( lookahead(comma_token) ) {
                match(comma_token);
                $$ = NodeFactory.List($1,parseNamedRestParameters());
            } else {
                $$ = $1;
            }
        }
    
        if( debug ) {
            Debugger.trace( "finish parseNamedRestParameters" );
        }

        return $$;
    }

    /**
     * NamedParameters	
     *     NamedParameter
     *     NamedParameter , NamedParameters
     */

    final public Node parseNamedParameters() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseNamedParameters" );
        }

        Node $$,$1;
        
        $1 = parseNamedParameter();
        if( lookahead(comma_token) ) {
            match(comma_token);
            $$ = NodeFactory.List($1,parseNamedParameters());
        } else {
            $$ = $1;
        }
    
        if( debug ) {
            Debugger.trace( "finish parseNamedParameters" );
        }

        return $$;
    }

    static final void testNamedParameters() throws Exception {

        String[] input = { 
	        "'a' x:String='default'",
	        "'a' x:String='default', 'b' y:Number,'c' z:Object" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("NamedParameters",input,expected);
    }

    /**
     * RestParameter	
     *     ...
     *     ... Parameter
     */

    final public Node parseRestParameter() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseRestParameter" );
        }

        Node $$,$1;
        
        match(tripledot_token);
        if( lookahead(identifier_token) ||
            lookahead(get_token) ||
            lookahead(set_token) ) {
            $1 = parseParameter();
        } else {
            $1 = null;
        }

        $$ = NodeFactory.RestParameter($1);

    
        if( debug ) {
            Debugger.trace( "finish parseRestParameter" );
        }

        return $$;
    }

    static final void testRestParameter() throws Exception {

        String[] input = { 
	        "...rest",
	        "...args" 
        };

        String[] expected = { 
            "restparameter( typedidentifier( identifier( rest ), null ), null )",
            "restparameter( typedidentifier( identifier( args ), null ), null )" 
        };

        testParser("RestParameter",input,expected);
    }

    /**
     * Parameter	
     *     Identifier
     *     Identifier : TypeExpression[allowIn]
     */

    final public Node parseParameter() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseParameter" );
        }

        Node $$,$1,$2;
        
        $1 = parseIdentifier();
        if( lookahead(colon_token) ) {
            match(colon_token);
            $2 = parseTypeExpression(allowIn_mode);
        } else {
            $2 = null;
        }

        $$ = NodeFactory.Parameter($1,$2);
    
        if( debug ) {
            Debugger.trace( "finish parseParameter" );
        }

        return $$;
    }

    static final void testParameter() throws Exception {

        String[] input = { 
	        "x",
	        "x:Object" 
        };

        String[] expected = { 
            "typedidentifier( identifier( x ), null )",
            "typedidentifier( identifier( x ), qualifiedidentifier( null, Object ) )" 
        };

        testParser("Parameter",input,expected);
    }

    /**
     * OptionalParameter	
     *     Parameter = AssignmentExpressionallowIn
     */

    final public Node parseOptionalParameter() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseOptionalParameter" );
        }

        Node $$,$1,$2;
        
        $1 = parseParameter();
        $$ = parseOptionalParameterPrime($1);

        if( debug ) {
            Debugger.trace( "finish parseOptionalParameter" );
        }

        return $$;
    }

    final public Node parseOptionalParameterPrime(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseOptionalParameterPrime" );
        }

        Node $$;
        
        match(assign_token);
        $$ = NodeFactory.OptionalParameter($1,parseAssignmentExpression(allowIn_mode));
    
        if( debug ) {
            Debugger.trace( "finish parseOptionalParameterPrime" );
        }

        return $$;
    }

    static final void testOptionalParameter() throws Exception {

        String[] input = { 
	        "x",
	        "x:Object" 
        };

        String[] expected = { 
            "typedidentifier( identifier( x ), null )",
            "typedidentifier( identifier( x ), qualifiedidentifier( null, Object ) )" 
        };

        testParser("OptionalParameter",input,expected);
    }

    /**
     * NamedParameter	
     *     Parameter
     *     OptionalParameter
     *     String NamedParameter
     */

    final public Node parseNamedParameter() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseNamedParameter" );
        }

        Node $$,$1,$2;
        
        if( lookahead(stringliteral_token) ) {
            $1 = NodeFactory.LiteralString(scanner.getTokenText(match(stringliteral_token)));
            $$ = NodeFactory.NamedParameter($1,parseNamedParameter());
        } else {
            $1 = parseParameter();
            if( lookahead(assign_token) ) {
                $$ = parseOptionalParameterPrime($1);
            } else {
                $$ = $1;
            }
        }

        if( debug ) {
            Debugger.trace( "finish parseNamedParameter" );
        }

        return $$;
    }

    static final void testNamedParameter() throws Exception {

        String[] input = { 
	        "x",
	        "x:Object" 
        };

        String[] expected = { 
            "typedidentifier( identifier( x ), null )",
            "typedidentifier( identifier( x ), qualifiedidentifier( null, Object ) )" 
        };

        testParser("NamedParameter",input,expected);
    }

    /**
     * ResultSignature	
     *     «empty»
     *     : TypeExpression[allowIn]
     */

    final public Node parseResultSignature() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseResultSignature" );
        }

        Node $$;
        
        if( lookahead(colon_token) ) {
            match(colon_token);
            $$ = parseTypeExpression(allowIn_mode);
        } else {
            $$ = null;
        }

        if( debug ) {
            Debugger.trace( "finish parseResultSignature" );
        }

        return $$;
    }

    static final void testResultSignature() throws Exception {

        String[] input = { 
	        ": String",
	        ": Object" 
        };

        String[] expected = { 
            "qualifiedidentifier( null, String )",
            "qualifiedidentifier( null, Object )" 
        };

        testParser("ResultSignature",input,expected);
    }

    /**
     * ClassDefinition	
     *     class Identifier Inheritance Block
     *     class Identifier Semicolon
     */

    final public Node parseClassDefinition(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseClassDefinition" );
        }

        Node $$,$1;

        match(class_token);
        $1 = parseIdentifier();

        if( lookaheadSemicolon(mode) ) {
            matchSemicolon(mode);
            $$ = NodeFactory.ClassDeclaration($1);
        } else {
        
            Node $2,$3;

            $2 = parseInheritance();
            $3 = parseBlock();
            $$ = NodeFactory.ClassDefinition($1,$2,$3);

        }
        
            
        if( debug ) {
            Debugger.trace( "finish parseClassDefinition" );
        }

        return $$;
    }

    static final void testClassDefinition() throws Exception {

        String[] input = { 
	        "class C;",
	        "class C {}",
            "class C extends B {}",
            "class C implements I,J {}",
            "class C extends B implements I {}"
        };

        String[] expected = { 
            "classdeclaration( identifier( C ) )",
            "classdefinition( identifier( C ), inheritance( null, null ), null )",
            "classdefinition( identifier( C ), inheritance( superclass( qualifiedidentifier( null, B ) ), null ), null )",
            "classdefinition( identifier( C ), inheritance( null, list( list( null, qualifiedidentifier( null, I ) ), qualifiedidentifier( null, J ) ) ), null )",
            "classdefinition( identifier( C ), inheritance( superclass( qualifiedidentifier( null, B ) ), list( null, qualifiedidentifier( null, I ) ) ), null )"
        };

        testParser("ClassDefinition",input,expected);
    }

    /**
     * Inheritance	
     *     «empty»
     *     extends TypeExpressionallowIn
     *     implements TypeExpressionList
     *     extends TypeExpressionallowIn implements TypeExpressionList
     */

    final public Node parseInheritance() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseInheritance" );
        }

        Node $$,$1,$2;
        
        if( lookahead(extends_token) ) {
            match(extends_token);
            $1 = parseTypeExpression(allowIn_mode);
        } else {
            $1 = null;
        }
    
        if( lookahead(implements_token) ) {
            match(implements_token);
            $2 = parseTypeExpressionList();
        } else {
            $2 = null;
        }

        $$ = NodeFactory.Inheritance($1,$2);

        if( debug ) {
            Debugger.trace( "finish parseInheritance" );
        }

        return $$;
    }

    static final void testSuperclass() throws Exception {

        String[] input = { 
	        "",
            "" 
        };

        String[] expected = { 
	        "",
            "" 
        };

        testParser("Superclass",input,expected);
    }

    /**
     * ImplementsList	
     *     «empty»
     *     implements TypeExpressionList
     */

    final public Node parseImplementsList() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseImplementsList" );
        }

        Node $$;
        
        if( lookahead(implements_token) ) {
            match(implements_token);
            $$ = parseTypeExpressionList();
        } else {
            $$ = null;
        }
    
        if( debug ) {
            Debugger.trace( "finish parseImplementsList" );
        }

        return $$;
    }

    static final void testImplementsList() throws Exception {

        String[] input = { 
	        "",
            "" 
        };

        String[] expected = { 
	        "",
            "" 
        };

        testParser("ImplementsList",input,expected);
    }

    /**
     * TypeExpressionList	
     *     TypeExpression[allowin] TypeExpressionListPrime
     *
     * TypeExpressionListPrime:	
     *     , TypeExpression[allowin] TypeExpressionListPrime
     *     empty
     */

    final public Node parseTypeExpressionList() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseTypeExpressionList" );
        }

        Node $$, $1;
        
        $1 = parseTypeExpression(allowIn_mode);
        $$ = parseTypeExpressionListPrime(NodeFactory.List(null,$1));

        if( debug ) {
            Debugger.trace( "finish parseTypeExpressionList" );
        }

        return $$;
    }

    private Node parseTypeExpressionListPrime( Node $1 ) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseTypeExpressionListPrime" );
        }

        Node $$;

        if( lookahead( comma_token ) ) {

            match( comma_token );

            Node $2;
        
            $2 = parseTypeExpression(allowIn_mode);
            $$ = parseTypeExpressionListPrime(NodeFactory.List($1,$2));

        } else {
            $$ = $1;
        }

        if( debug ) {
            Debugger.trace( "finish parseTypeExpressionListPrime'" );
        }

        return $$;
    }

    static final void testTypeExpressionList() throws Exception {

        String[] input = { 
	        "I1",
	        "I1,I2" 
        };

        String[] expected = { 
            "",
            "" 
        };

        testParser("TypeExpressionList",input,expected);
    }

    /**
     * InterfaceDefinition	
     *     interface Identifier ExtendsList Block
     *     interface Identifier Semicolon
     */

    final public Node parseInterfaceDefinition(int mode) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseInterfaceDefinition" );
        }

        Node $$,$1;

        match(interface_token);
        $1 = parseIdentifier();
        if( lookaheadSemicolon(mode) ) {
            matchSemicolon(mode);
            $$ = NodeFactory.InterfaceDeclaration($1);
        } else {
            Node $2,$3;
            $2 = parseExtendsList();
            $3 = parseBlock();
            $$ = NodeFactory.InterfaceDefinition($1,$2,$3);
        }
            
        if( debug ) {
            Debugger.trace( "finish parseInterfaceDefinition" );
        }

        return $$;
    }

    static final void testInterfaceDefinition() throws Exception {

        String[] input = { 
        };

        String[] expected = { 
        };

        testParser("InterfaceDefinition",input,expected);
    }

    /**
     * ExtendsList	
     *     «empty»
     *     extends TypeExpressionList
     */

    final public Node parseExtendsList() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseExtendsList" );
        }

        Node $$;
        
        if( lookahead(extends_token) ) {
            match(extends_token);
            $$ = parseTypeExpressionList();
        } else {
            $$ = null;
        }
    
        if( debug ) {
            Debugger.trace( "finish parseExtendsList" );
        }

        return $$;
    }

    static final void testExtendsList() throws Exception {

        String[] input = { 
	        "",
            "" 
        };

        String[] expected = { 
	        "",
            "" 
        };

        testParser("ExtendsList",input,expected);
    }

    /**
     * NamespaceDefinition	
     *     namespace Identifier ExtendsList
     */

    final public Node parseNamespaceDefinition() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseNamespaceDefinition" );
        }

        Node $$,$1,$2;
        
        match(namespace_token);
        $1 = parseIdentifier();
        $2 = parseExtendsList();
        $$ = NodeFactory.NamespaceDefinition($1,$2);
            
        if( debug ) {
            Debugger.trace( "finish parseNamespaceDefinition" );
        }

        return $$;
    }

    static final void testNamespaceDefinition() throws Exception {

        String[] input = { 
	        "namespace N;",
        };

        String[] expected = { 
            "",
        };

        testParser("NamespaceDefinition",input,expected);
    }

    /**
     * LanguageDeclaration	
     *     use LanguageAlternatives
     */

    public final Node parseLanguageDeclaration() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseLanguageDeclaration" );
        }

        Node $$;

        match(use_token);
        $$ = NodeFactory.LanguageDeclaration(parseLanguageAlternatives());

        if( debug ) {
            Debugger.trace( "finish parseLanguageDeclaration" );
        }

        return $$;
    }

    /**
     * LanguageAlternatives	
     *     LanguageIds
     *     LanguageAlternatives | LanguageIds
     */

    final public Node parseLanguageAlternatives() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseLanguageAlternatives" );
        }

        Node $$;

        $$ = parseLanguageIds();

        while( lookahead(bitwiseor_token) ) {

            match(bitwiseor_token);
            $$ = NodeFactory.List($$,parseLanguageIds());

        }

        if( debug ) {
            Debugger.trace( "finish parseLanguageAlternatives" );
        }

        return $$;
    }

    /**
     * LanguageIds	
     *     «empty»
     *     LanguageId LanguageIds
     */

    final public Node parseLanguageIds() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseLanguageIds" );
        }

        Node $$;

        if( lookahead(bitwiseor_token) || lookahead(semicolon_token) ) {
            $$ = null;
        } else {
            $$ = NodeFactory.List(parseLanguageId(),parseLanguageIds());
        }

        if( debug ) {
            Debugger.trace( "finish parseLanguageIds" );
        }

        return $$;
    }

    /**
     * LanguageId	
     *     Identifier
     *     Number
     */

    final public Node parseLanguageId() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseLanguageId" );
        }

        Node $$;
        
        if( lookahead(numberliteral_token) ) {
            $$ = NodeFactory.LiteralNumber(scanner.getTokenText(match(numberliteral_token)));
        } else {
            $$ = parseIdentifier();
        }

        if( debug ) {
            Debugger.trace( "finish parseLanguageId" );
        }

        return $$;
    }

    /**
     * PackageDefinition	
     *     package Block
     *     package PackageName Block
     */

    final public Node parsePackageDefinition() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parsePackageDefinition" );
        }

        Node $$;
        
        match(package_token);
        if( lookahead(leftbrace_token) ) {
            $$ = NodeFactory.PackageDefinition(null,parseBlock());
        } else {
            $$ = NodeFactory.PackageDefinition(parsePackageName(),parseBlock());
        }
            
        if( debug ) {
            Debugger.trace( "finish parsePackageDefinition" );
        }

        return $$;
    }

    static final void testPackageDefinition() throws Exception {

        String[] input = { 
	        "package {}",
	        "package P {}",
        };

        String[] expected = { 
            "",
            "",
        };

        testParser("PackageDefinition",input,expected);
    }

    /**
     * PackageName	
     *     Identifier
     *     PackageName . Identifier
     */

    final public Node parsePackageName() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parsePackageName" );
        }

        Node $$,$1;

        $1 = parseIdentifier();
        $$ = parsePackageNamePrime($1);

        if( debug ) {
            Debugger.trace( "finish parsePackageName" );
        }

        return $$;
    }

    final public Node parsePackageNamePrime(Node $1) throws Exception {

        if( debug ) {
            Debugger.trace( "begin parsePackageNamePrime" );
        }

        Node $$;

        while( lookahead(dot_token) ) {
            match(dot_token);
            $1 = NodeFactory.List($1,parseIdentifier());
        }

        $$ = $1;

        if( debug ) {
            Debugger.trace( "finish parsePackageNamePrime" );
        }

        return $$;
    }

    /**
     * Program	
     *     TopStatements
     */

    final public Node parseProgram() throws Exception {

        if( debug ) {
            Debugger.trace( "begin parseProgram" );
        }

        Node $$;
        
		$$ = parseTopStatements();
            
        if( debug ) {
            Debugger.trace( "finish parseProgram" );
        }

		if( scanner.errorCount == 0 ) {
		    $$ = NodeFactory.Program($$);
		} else {
            $$ = NodeFactory.Program(null);
        }
        return $$;
    }

    static final void testProgram() throws Exception {

        String[] input = { 
	        "function f() {} interface I { function m(); } class C extends B implements I,J,K { static function g() : I { return new C; } function m() {} } C.g().f();",
            "package P { include 'file1'; import 'lib1'; namespace V1; const v1 = V1; v1 var x; }",
        };

        String[] expected = { 
            "program( statementlist( statementlist( statementlist( statementlist( statementlist( statementlist( statementlist( null, annotateddefinition( null, functiondefinition( functiondeclaration( functionname( <empty>, identifier( f ) ), functionsignature( null, null ) ), null ) ) ), annotateddefinition( null, interfacedefinition( identifier( I ), null, statementlist( statementlist( statementlist( null, annotateddefinition( null, functiondeclaration( functionname( <empty>, identifier( m ) ), functionsignature( null, null ) ) ) ), null ), null ) ) ) ), annotateddefinition( null, classdefinition( identifier( C ), inheritance( superclass( qualifiedidentifier( null, B ) ), list( list( list( null, qualifiedidentifier( null, I ) ), qualifiedidentifier( null, J ) ), qualifiedidentifier( null, K ) ) ), statementlist( statementlist( statementlist( statementlist( null, annotateddefinition( attributelist( identifier( static ), null ), functiondefinition( functiondeclaration( functionname( <empty>, identifier( g ) ), functionsignature( null, qualifiedidentifier( null, I ) ) ), statementlist( statementlist( statementlist( statementlist( null, null:returnstatement( newexpression( qualifiedidentifier( null, C ) ) ) ), emptystatement ), null ), null ) ) ) ), annotateddefinition( null, functiondefinition( functiondeclaration( functionname( <empty>, identifier( m ) ), functionsignature( null, null ) ), null ) ) ), null ), null ) ) ) ), callexpression( memberexpression( callexpression( memberexpression( qualifiedidentifier( null, C ), qualifiedidentifier( null, g ) ), null ), qualifiedidentifier( null, f ) ), null ) ), emptystatement ), null ), null ) )",
            "program( statementlist( statementlist( statementlist( null, packagedefinition( identifier( P ), statementlist( statementlist( statementlist( statementlist( statementlist( statementlist( statementlist( statementlist( statementlist( null, includestatement( literalstring( file1 ) ) ), emptystatement ), annotateddefinition( null, importdefinition( literalstring( lib1 ), null ) ) ), emptystatement ), annotateddefinition( null, namespacedefinition( identifier( V1 ), null ) ) ), annotateddefinition( null, variabledefinition( const_token, list( null, variablebinding( typedvariable( identifier( v1 ), null ), qualifiedidentifier( null, V1 ) ) ) ) ) ), annotateddefinition( attributelist( qualifiedidentifier( null, v1 ), null ), variabledefinition( var_token, list( null, variablebinding( typedvariable( identifier( x ), null ), null ) ) ) ) ), null ), null ) ) ), null ), null ) )",
        };

        testParser("Program",input,expected);
    }

}

/*
 * The end.
 */
