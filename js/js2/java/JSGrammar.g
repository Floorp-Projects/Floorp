/*
	TODO:

		* Add semantic feedback to lexer for:
		    * JS 1.x regexps
			* Capturing blocks that have syntax errors
		* Add semicolon abbreviation
		* Ensure that optional function parameters do not precede required function parameters
*/

{
import java.io.*;
// Test program
class TestMain {
	public static void main(String[] args) {
		try {
			JSLexer lexer = new JSLexer(new DataInputStream(System.in));
			JSParser parser = new JSParser(lexer);
			parser.program();
		} catch(Exception e) {
			System.err.println("exception: "+e);
		}
	}
}
}

class JSParser extends Parser;

options {
//	k = 2;                           // two token lookahead
	tokenVocabulary=JS;            	 // Call its vocabulary "JS"
	codeGenMakeSwitchThreshold = 2;  // Some optimizations
	codeGenBitsetTestThreshold = 3;
//	defaultErrorHandler = false;     // Don't generate parser error handlers
//	buildAST = true;
}

{
	// Possible scopes for parsing
	static final int TopLevelScope = 0;
	static final int ClassScope = 1;
	static final int BlockScope = 2;
}

// ********* Identifiers **********
identifier returns [ExpressionNode e]
    { e = null; }
	:	opI:IDENT       { e = new JSObject(opI.getText()); }
	|	"version"       { e = new JSObject("version"); }
	|	"override"      { e = new JSObject("override"); }
	|	"method"        { e = new JSObject("method"); }
	|	"getter"        { e = new JSObject("getter"); }
	|	"setter"        { e = new JSObject("setter"); }
	|	"traditional"   { e = new JSObject("traditional"); }
	|	"constructor"   { e = new JSObject("constructor"); }
	;

qualified_identifier returns [ExpressionNode e]
    { e = null; ExpressionNode e2 = null; }
	:	(e2 = parenthesized_expression)? ("::" e = identifier { e = new BinaryNode("::", e2, e); } )+
	|	e = identifier ("::" e2 = identifier { e = new BinaryNode("::", e, e2); } )*
	;

qualified_identifier_or_parenthesized_expression returns [ExpressionNode e]
    { e = null; ExpressionNode e2 = null; }
	:	(e = parenthesized_expression | e = identifier) ("::" e2 = identifier { e = new BinaryNode("::", e, e2); } )*
	;

// ********* Primary Expressions **********
primary_expression[boolean initial] returns [ExpressionNode e]
    { e = null; }
	:	{!initial}?
		(
			e = function_expression
		|	e = object_literal
		)
	|	e = simple_expression
	;

simple_expression returns [ExpressionNode e]
    { e = null; }
	:	"null"      { e = new JSObject("null"); }
	|	"true"      { e = JSBoolean.JSTrue; }
	|	"false"     { e = JSBoolean.JSFalse; }
	|	opN:NUMBER  { e = new JSDouble(opN.getText()); }
	|	opS:STRING  { e = new JSString(opS.getText()); }
	|	"this"      { e = new JSObject("this"); }
	|	"super"     { e = new JSObject("super"); }
	|	e = qualified_identifier_or_parenthesized_expression
	|	opR:REGEXP  { e = new JSObject(opR.getText()); }
	|	e = array_literal
	;

parenthesized_expression returns [ExpressionNode e]
    { e = null; }
	:	"(" e = expression[false, true] ")"	;

// ********* Function Expressions **********
function_expression returns [ExpressionNode e]
    { e = null; }
	:	e = function[false]
	;

// ********* Object Literals **********
object_literal returns [ExpressionNode e]
    { e = null; }
	:	"{" (e = field_list)? "}" ;

field_list returns [ExpressionNode e]
    { e = null; ExpressionNode e2 = null; }
	:	e = literal_field ("," e2 = literal_field  { e = new BinaryNode(",", e, e2); } )* ;

literal_field returns [ExpressionNode e]
    { e = null; ExpressionNode e2 = null; }
	:	e = qualified_identifier ":" e = assignment_expression[false, true] { e = new BinaryNode(":", e, e2); }
	;

// ********* Array Literals **********
array_literal returns [ExpressionNode e]
    { e = null; }
	:	"[" (element_list)? "]"	;

element_list
	:	literal_element ("," literal_element)* ;

literal_element
    { ExpressionNode e = null; }
	:	e = assignment_expression[false, true] ;

// ********* Postfix Unary Operators **********
postfix_expression[boolean initial] returns [ExpressionNode e]
    { e = null; ExpressionNode r = null; }
	:
	(	e = primary_expression[initial]
	|	e = new_expression
	)
	(
		r = member_operator { e = new BinaryNode(((UnaryNode)r).getOperator(), e, ((UnaryNode)r).getChild()); }
	|	r = arguments { e = new BinaryNode("()", e, r); }
	|	"++" { e = new UnaryNode("++", e); }
	|	"--" { e = new UnaryNode("--", e); }
	)*
	;

new_expression returns [ExpressionNode e]
    { e = null; }
	:	"new"
		(
			e = new_expression
		|	(
				e = primary_expression[false]
				(
					// There's an ambiguity here:
					//  Consider the input 'new f.x'.  Is that equivalent to '(new f).x' or 'new (f.x)' ?
					//  Assume the latter by using ANTLR's default behavior of consuming input as
					//  soon as possible and quell the resultant warning.
					options {
                    	warnWhenFollowAmbig=false;
                    }
				:	e = member_operator
				)*
			)
		)
		(
			// There's an ambiguity here:
			//  Consider the input 'new F(arg)'.  Is that equivalent to '(new F)(arg)' or does
			//  it construct an instance of F, passing 'arg' as an argument to the constructor ?
			//  Assume the latter by using ANTLR's default behavior of consuming input as
			//  soon as possible and quell the resultant warning.
			options {
               	warnWhenFollowAmbig=false;
            }
		:	e = arguments
		)*
	;

member_operator returns [ExpressionNode e]
    { e = null; }
	:	"[" e = argument_list "]" { e = new UnaryNode("[]", e); }
	|	DOT e = qualified_identifier_or_parenthesized_expression { e = new UnaryNode(".", e); }
	|	"@" e = qualified_identifier_or_parenthesized_expression { e = new UnaryNode("@", e); }
	;

arguments returns [ExpressionNode e]
    { e = null; }
	:	"(" e = argument_list ")" ;

argument_list returns [ExpressionNode e]
    { e = null; ExpressionNode e2 = null; }
	:	(e = assignment_expression[false, true] (","  e2 = assignment_expression[false, true] { e = new BinaryNode(",", e, e2); } )*)? ;

// ********* Prefix Unary Operators **********
unary_expression[boolean initial] returns [ExpressionNode e]
    { e = null; }
	:	e = postfix_expression[initial]
	|	"delete" e = postfix_expression[false] { e = new UnaryNode("delete", e); }
	|	"typeof" e = unary_expression[false] { e = new UnaryNode("typeof", e); }
	|	"eval" e = unary_expression[false] { e = new UnaryNode("eval", e); }
	| 	"++" e = postfix_expression[false] { e = new UnaryNode("++", e); }
	|	"--" e = postfix_expression[false] { e = new UnaryNode("--", e); }
	|	"+" e = unary_expression[false] { e = new UnaryNode("+", e); }
	|	"-" e = unary_expression[false] { e = new UnaryNode("-", e); }
	|	"~" e = unary_expression[false] { e = new UnaryNode("~", e); }
	|	"!" e = unary_expression[false] { e = new UnaryNode("!", e); }
	;

// ********* Multiplicative Operators **********
multiplicative_expression[boolean initial] returns [ExpressionNode e]
    { e = null;  ExpressionNode r = null; }
	:	e = unary_expression[initial] (
	    ("*" r = unary_expression[false] { e = new ArithmeticNode("*", e, r); } )
	|	("/" r = unary_expression[false] { e = new ArithmeticNode("/", e, r); } )
    |	("%" r = unary_expression[false] { e = new ArithmeticNode("%", e, r); } )
        )*
	;

// ********* Additive Operators **********
additive_expression[boolean initial] returns [ExpressionNode e]
    { e = null; ExpressionNode r = null; }
	:	e = multiplicative_expression[initial] (
	    ("+" r = multiplicative_expression[false] { e = new ArithmeticNode("+", e, r); } )
	|	("-" r = multiplicative_expression[false] { e = new ArithmeticNode("-", e, r); } )
	    )*
	;

// ********* Bitwise Shift Operators **********
shift_expression[boolean initial] returns [ExpressionNode e]
    { e = null;  ExpressionNode r = null; }
	:	e = additive_expression[initial] (
	    ("<<" r = additive_expression[false] { e = new BitwiseNode("<<", e, r); } )
	|	(">>" r = additive_expression[false] { e = new BitwiseNode(">>", e, r); } )
	|	(">>>" r = additive_expression[false] { e = new BitwiseNode(">>>", e, r); } )
	    )*
	;

// ********* Relational Operators **********
relational_operator[boolean allowIn] returns [String s]
    { s = null; }
	:	{allowIn}? "in" { s = "in"; }
	|	"<"             { s = "<"; }
	|	">"             { s = ">"; }
	| 	"<="            { s = "<="; }
	| 	">="            { s = ">="; }
	| 	"instanceof"    { s = "instanceof"; }
	;

relational_expression[boolean initial, boolean allowIn] returns [ExpressionNode e]
    { e = null; ExpressionNode r = null; String op = null; }
	:	e = shift_expression[initial]
		(
			// ANTLR reports an ambiguity here because the FOLLOW set of a relational_expression
			// includes the "in" token (from the for-in statement), but there's no real ambiguity
			// here because the 'allowIn' semantic predicate is used to prevent the two from being
			// confused.
			options { warnWhenFollowAmbig=false; }:
			op = relational_operator[allowIn] r = shift_expression[false] { e = new RelationalNode(op, e, r); }
		)*
	;

// ********* Equality Operators **********
equality_expression[boolean initial, boolean allowIn] returns [ExpressionNode e]
    { e = null; ExpressionNode r = null; }
	:	e = relational_expression[initial, allowIn]
	    (
		    ("==" r = relational_expression[false, allowIn]) { e = new RelationalNode("==", e, r); }
		|   ("!=" r = relational_expression[false, allowIn]) { e = new RelationalNode("!=", e, r); }
		|   ("===" r = relational_expression[false, allowIn]) { e = new RelationalNode("===", e, r); }
		|   ("!==" r = relational_expression[false, allowIn]) { e = new RelationalNode("!===", e, r); }
		)*
	;

// ********* Binary Bitwise Operators **********
bitwise_and_expression[boolean initial, boolean allowIn] returns [ExpressionNode e]
    { e = null; ExpressionNode r = null; }
	:	e = equality_expression[initial, allowIn]
		("&" r = equality_expression[false, allowIn] { e = new BitwiseNode("&", e, r); } )*
	;

bitwise_xor_expression[boolean initial, boolean allowIn] returns [ExpressionNode e]
    { e = null; ExpressionNode r = null; }
	:	e = bitwise_and_expression[initial, allowIn]
		("^" (r = bitwise_and_expression[false, allowIn] { e = new BitwiseNode("^", e, r); } | "*" | "?"))*
	;

bitwise_or_expression[boolean initial, boolean allowIn] returns [ExpressionNode e]
    { e = null; ExpressionNode r = null; }
	:	e = bitwise_xor_expression[initial, allowIn]
		("|" (r = bitwise_xor_expression[false, allowIn] { e = new BitwiseNode("|", e, r); } | "*" | "?"))*
	;

// ********* Binary Logical Operators **********
logical_and_expression[boolean initial, boolean allowIn] returns [ExpressionNode e]
    { e = null; ExpressionNode r = null; }
	:	e = bitwise_or_expression[initial, allowIn]
	    ("&&" r = bitwise_or_expression[false, allowIn] { e = new LogicalNode("&&", e, r); } )*
	;

logical_or_expression[boolean initial, boolean allowIn] returns [ExpressionNode e]
    { e = null; ExpressionNode r = null; }
	:	e = logical_and_expression[initial, allowIn]
	    ("||" r = logical_and_expression[false, allowIn] { e = new LogicalNode("||", e, r); } )*
	;

// ********* Conditional Operators **********
conditional_expression[boolean initial, boolean allowIn] returns [ExpressionNode e]
    { e = null; ExpressionNode e1 = null; ExpressionNode e2 = null; }
	:	e = logical_or_expression[initial, allowIn]
		// There's an ambiguity here:
		//  Consider the input 'a ? b : c ? d : e'.  Is that equivalent to '(a ? b : c) ? d : e' or
		//  should it be interpreted as 'a ? b : (c ? d : e)' ?
		//  Assume the latter by using ANTLR's default behavior of consuming input as
		//  soon as possible and quell the resultant warning.
		(
			options {warnWhenFollowAmbig=false;}
		:	"?" e1 = assignment_expression[false, allowIn] ":" e2 = assignment_expression[false, allowIn]
    		{
    		    e = new BinaryNode("?", e, new BinaryNode(":", e1, e2));
    		}
		)*
	;

non_assignment_expression[boolean initial, boolean allowIn] returns [ExpressionNode e]
    { e = null; ExpressionNode e1 = null; ExpressionNode e2 = null; }
	:	e = logical_or_expression[initial, allowIn]
		// There's an ambiguity here:
		//  Consider the input 'a ? b : c ? d : e'.  Is that equivalent to '(a ? b : c) ? d : e' or
		//  should it be interpreted as 'a ? b : (c ? d : e)' ?
		//  Assume the latter by using ANTLR's default behavior of consuming input as
		//  soon as possible and quell the resultant warning.
		(
			options {
           		warnWhenFollowAmbig=false;
            }
		:	"?" e1 = non_assignment_expression[false, allowIn] ":" e2 = non_assignment_expression[false, allowIn]
    		{
    		    e = new BinaryNode("?", e, new BinaryNode(":", e1, e2));
    		}
		)*
	;

// ********* Assignment Operators **********

// FIXME - Can we get rid of lookahead ?
assignment_expression[boolean initial, boolean allowIn] returns [ExpressionNode e]
    { e = null; ExpressionNode p = null; ExpressionNode a = null; String op = null; }
	:	(postfix_expression[initial] "=") =>
    	    	(p = postfix_expression[initial] "=" a = assignment_expression[false, allowIn] { e = new AssignmentNode("=", p, a); } )
	|	(postfix_expression[initial] compound_assignment) =>
        	    (p = postfix_expression[initial] op = compound_assignment a = assignment_expression[false, allowIn] { e = new AssignmentNode(op, p, a); } )
	|	e = conditional_expression[false, allowIn]
	;

compound_assignment returns [String op]
    { op = null; }
	:	"*="    { op = "*="; }
	|	"/="    { op = "/="; }
	|	"%="    { op = "%="; }
	|	"+="    { op = "+="; }
	|	"-="    { op = "-="; }
	|	"<<="   { op = "<<="; }
	|	">>="   { op = ">>="; }
	|	">>>="  { op = ">>>="; }
	|	"&="    { op = "&="; }
	|	"^="    { op = "^="; }
	|	"|="    { op = "|="; }
	;

// ********* Expressions **********
expression[boolean initial, boolean allowIn] returns [ExpressionNode e]
    { e = null; ExpressionNode e2 = null; }
	:	e = assignment_expression[initial, allowIn] ("," e2 = assignment_expression[false, allowIn] { e = new BinaryNode(",", e, e2); } )*
	;

optional_expression returns [ExpressionNode e]
    { e = null; }
	:	( e = expression[false, true])?
	;

// ********* Type Expressions **********
type_expression[boolean initial, boolean allowIn]
    { ExpressionNode e = null; }
	:	e = non_assignment_expression[initial, allowIn]
	;

// ********* Statements **********

statement[int scope, boolean non_empty, ControlNodeGroup container]
	:	(definition[scope]) => definition[scope]
	|	code_statement[non_empty, container, null]
	;

code_statement[boolean non_empty, ControlNodeGroup container, String label]
	:	empty_statement[non_empty]

// Bogus predicate required to eliminate ANTLR nondeterminism warning
// on lookahead of '{' between expression_statement and block, even
// though the symantic predicate in the primary_expression rule disambiguates
// the two.
	|	("{") => block[BlockScope, container]
	|	(identifier ":") => labeled_statement[non_empty, container]
	|	expression_statement[container] semicolon
	|	if_statement[non_empty, container]
	|	switch_statement[container]
	|	do_statement[container, label] semicolon
	|	while_statement[non_empty, container, label]
	|	for_statement[non_empty, container, label]
	|	with_statement[non_empty, container]
	|	continue_statement[container] semicolon
	|	break_statement[container] semicolon
	|	return_statement[container] semicolon
	|	throw_statement[container] semicolon
	|	try_statement[container]
	|	import_statement[non_empty]
	;

semicolon
// FIXME - Add abbreviation
	:	";"
	;

// ********* Empty Statement **********
// FIXME
empty_statement[boolean non_empty]
	:	";"
	;

// ********* Expression Statement **********
expression_statement[ControlNodeGroup container]
    { ExpressionNode e = null; }
	:	e = expression[true, true]
	    {
	        container.add(new ControlNode(e));
	    }

	;

// ********* Block **********
block[int scope, ControlNodeGroup container]
    { ControlNodeGroup subContainer = new ControlNodeGroup(); }
	:	"{"
	        statements[scope, subContainer]
	        {
	            container.add(subContainer);    // necessary ?
	        }
	    "}"
	;

// FIXME
statements[int scope, ControlNodeGroup container]
	:	 statement[scope, false, container]
	        ( statement[scope, false, container] )*
	;

// ********* Labeled Statements **********
labeled_statement[boolean non_empty, ControlNodeGroup container]
    { ExpressionNode e = null; }
	:	e = identifier ":" code_statement[non_empty, container, ((JSObject)e).value]
	;

if_statement[boolean non_empty, ControlNodeGroup container]
    { ConditionalNode condition = null; ExpressionNode e = null; }
	:	"if"  e = parenthesized_expression
	    {
	        condition = new ConditionalNode(e);
	        container.add(condition);
	    }
	    code_statement[non_empty, container, null]
	    {
            condition.moveNextToTrue();
            container.addTail(condition);
	    }
	    (
    		// Standard if/else ambiguity
	        options { warnWhenFollowAmbig=false; }:
			"else" code_statement[non_empty, container, null]
    	)?
	;

// ********* Switch statement **********
switch_statement[ControlNodeGroup container]
    { ExpressionNode e = null; SwitchNode s = null; ControlNodeGroup c = new ControlNodeGroup(); }
	:	"switch" e = parenthesized_expression
	    {
	        s = new SwitchNode(e);
	    }
	    "{" (case_groups[s, c])? "}"
	    {
	        container.add(s);
	        c.shiftBreakTails(null);
	        container.addTails(c);
	    }
	;

case_groups[SwitchNode s, ControlNodeGroup container]
	:	(case_group[s, container])+
	;

case_group[SwitchNode s, ControlNodeGroup container]
    { ExpressionNode e = null; ControlNodeGroup c = new ControlNodeGroup(); }
	:	( e = case_guard)+ (code_statement[true, c, null])+
	{
	    s.addCase(e, c.getHead());
	    container.add(c);
	}
	;

case_guard returns [ExpressionNode e]
    { e = null; }
	:	"case" e = expression[false, true] ":"
	|	"default" ":"
	;

// FIXME
case_statements
    { ControlNodeGroup c = new ControlNodeGroup(); }
	:	( code_statement[false, c, null] )+
	;

// ********* Do-While statement **********
do_statement[ControlNodeGroup container, String label]
    { ControlNodeGroup body = new ControlNodeGroup(); ExpressionNode e = null; }
	:	"do" code_statement[true, body, null] "while" e = parenthesized_expression
	{
	    ConditionalNode condition = new ConditionalNode(e, body.getHead());
	    body.fixTails(condition);
	    body.fixContinues(condition, label);
	    body.shiftBreakTails(label);
        container.add(body);
	}
	;

// ********* While statement **********
while_statement[boolean non_empty, ControlNodeGroup container, String label]
    { ExpressionNode e = null; ControlNodeGroup body = new ControlNodeGroup(); }
	:	"while" e = parenthesized_expression code_statement[non_empty, body, null]
	    {
	        ConditionalNode condition = new ConditionalNode(e, body.getHead());
            body.fixTails(condition);
            body.fixContinues(condition, label);
            body.shiftBreakTails(label);
            body.setHead(condition);
            container.add(body);
	    }
	;

// ********* For statement **********
for_statement[boolean non_empty, ControlNodeGroup container, String label]
    {
        ExpressionNode ei = null;
        ExpressionNode ec = null;
        ExpressionNode en = null;
        ControlNodeGroup body = new ControlNodeGroup();
    }
	:	"for" "("
		(
			(for_initializer ";") => ei = for_initializer ";" ec = optional_expression ";" en = optional_expression
		|	ei = for_in_binding "in" ec = expression[false, true]
		)
		")"
		code_statement[non_empty, body, null]
		{
		    container.add(new ControlNode(ei));

		    ControlNode increment = new ControlNode(en);
		    body.fixContinues(increment, label);
		    body.add(increment);

		    ControlNode condition = new ConditionalNode(ec, body.getHead());
		    body.fixTails(condition);
		    body.shiftBreakTails(label);
            body.setHead(condition);

		    container.add(body);

		}
	;

for_initializer returns [ExpressionNode e]
    { e = null; }
	:	(
			e = expression[false, true]
		|	("var" | "const") variable_binding_list[false]
		)?
	;

for_in_binding returns [ExpressionNode e]
    { e = null; }
	:	(
			e = postfix_expression[false]
		|	("var" | "const") variable_binding_list[false]
		)
	;

// ********* With statement **********
with_statement[boolean non_empty, ControlNodeGroup container]
    { ExpressionNode e = null; ControlNodeGroup body = new ControlNodeGroup(); }
	:	"with" e = parenthesized_expression code_statement[non_empty, body, null]
	    {
	        container.add(new ControlNode(e));
	        container.add(body);
	    }
	;

// ********* Continue and Break statement **********
continue_statement [ControlNodeGroup container]
    { ExpressionNode e = null; }
	:	"continue" (e = identifier)?
	{
	    container.addContinue(new ControlNode(e));
	}
	;

break_statement [ControlNodeGroup container]
    { ExpressionNode e = null; }
	:	"break" (e = identifier)?
	{
	    container.addBreak(new ControlNode(e));
	}
	;

// ********* Return statement **********
return_statement [ControlNodeGroup container]
    { ExpressionNode e = null; }
	:	"return" e = optional_expression
	    {
	        container.add(new ControlNode(e));
	    }
	;

// ********* Throw statement **********
throw_statement [ControlNodeGroup container]
    { ExpressionNode e = null; }
	:	"throw" e = expression[false, true]
	    {
	        container.add(new ThrowNode(e));
	    }
	;

// ********* Try statement **********
try_statement [ControlNodeGroup container]
    {
        TryNode t = null;
        ControlNodeGroup tryBody = new ControlNodeGroup();
        ControlNodeGroup finallyBody = new ControlNodeGroup();
    }
	:	"try"	block[BlockScope, tryBody]
	    {
            t = new TryNode(tryBody.getHead());
	    }
	    (catch_clause[t])*
	    ("finally" block[BlockScope, finallyBody] { t.addFinally(finallyBody.getHead()); } )?
	    {
	        container.add(t);
	    }
	;

catch_clause [TryNode t]
    { ControlNodeGroup catchCode = new ControlNodeGroup(); ExpressionNode e = null; }
	:	"catch"	"(" e = typed_identifier[true] ")" block[BlockScope, catchCode]
	    {
	        t.addCatchClause(e, catchCode.getHead());
	    }
	;

// ********* Import statement **********
import_statement[boolean non_empty]
    { ControlNodeGroup c = new ControlNodeGroup(); }
	:	"import" import_list
		(
			";"
		|	block[BlockScope, c] ("else" code_statement[non_empty, c, null])
		)
	;

import_list
	:	import_item ("," import_item)*
	;

import_item
    { ExpressionNode e = null; }
	:	(identifier "=") => e = identifier "=" import_source
	|	import_source
	|	"protected" e = identifier "=" import_source
	;

import_source
    { ExpressionNode e = null; }
	:	e = non_assignment_expression[false, false] (":" Version)
	;

// ********* Definitions **********
definition[int scope]
	:	visibility global_definition
	|	local_definition[scope]
	;

global_definition
    { ExpressionNode e = null; }
	:	version_definition semicolon
	|	variable_definition semicolon

	// Syntactic predicate is required to disambiguate between getter/setter methods
	// and getter/setter functions
	|	("traditional" | "function" | (("getter" | "setter") "function")) => e = function_definition
	|	member_definition
	|	class_definition
	;

local_definition[int scope]
    { ExpressionNode e = null; }
	:	{scope == TopLevelScope || scope == ClassScope}? (class_definition | member_definition)
	|	variable_definition semicolon
	|	e = function_definition
	;

// ********* Visibility Specifications **********
visibility
	:	"private"
	|	"package"
	|	"public" versions_and_renames
	;

// ********* Versions **********

// FIXME
versions_and_renames:
	version_range_and_alias ("," version_range_and_alias)*
	;

version_range_and_alias
    { ExpressionNode e = null; }
	:	version_range (":" e = identifier)
	;

version_range
	:	(version)? (".." (version)?)
	;

version:	STRING ;

// ********* Version Definition **********
version_definition
	:	"version" version
		(
			">" version_list
			"=" version
		)?
	;

version_list
	:	version ("," version)*
	;

// ********* Variable Definition **********
variable_definition
	:	("var" | "const") variable_binding_list[true]
	;

variable_binding_list[boolean allowIn]
	:	variable_binding[allowIn] ("," variable_binding[allowIn])*
	;

variable_binding[boolean allowIn]
    { ExpressionNode e = null; }
	:	e = typed_identifier[allowIn] ("=" e = assignment_expression[false, allowIn])?
	;

typed_identifier[boolean allowIn] returns [ExpressionNode e]
    { e = null; }
	:	(type_expression[false, allowIn] identifier) => type_expression[false, allowIn] e = identifier
	|	e = identifier
	;

// ********* Function Definition **********
function_definition returns [ExpressionNode e]
    { e = null; }
	:	e = named_function
	|	"getter" e = named_function
	|	"setter" e = named_function
	|	traditional_function
	;

anonymous_function returns [ExpressionNode e]
    { e = null; }
	:	e = function[false]
	;

named_function returns [ExpressionNode e]
    { e = null; }
	:	e = function[true]
	;

function[boolean nameRequired] returns [ExpressionNode e]
    { e = null; ControlNodeGroup body = new ControlNodeGroup(); }
	:	"function"
		(
			{nameRequired}? e = identifier
		|	e = identifier
		)
		function_signature block[BlockScope, body]
	;

function_signature
	:	parameter_signature result_signature
	;

parameter_signature
	:	"(" parameters ")"
	;

parameters
	:	(parameter ("," parameter)*)? (rest_parameter)?
	;

// FIXME - Required parameters cannot follow optional parameters
parameter
    { ExpressionNode e = null; }
	:	e = typed_identifier[true] ("=" (e = assignment_expression[false, true])? )?
	;

rest_parameter
    { ExpressionNode e = null; }
	:	"..." (e = identifier)?
	;


result_signature
	:	(
			// ANTLR reports an ambiguity here because the FOLLOW set of a parameter_signature
			// includes the "{" token (from the block that contains a function's code), but there's
			// no real ambiguity here because the 'allowIn' semantic predicate is used to prevent the
			// two from being confused.
			options { warnWhenFollowAmbig=false; }:
			type_expression[true, true]
		)?
	;

traditional_function
    { ExpressionNode e = null; ControlNodeGroup c = new ControlNodeGroup(); }
	: "traditional" "function" e = identifier "(" traditional_parameter_list ")" block[BlockScope, c]
	;

traditional_parameter_list
    { ExpressionNode e = null; ExpressionNode e2 = null; }
    :	(e = identifier ("," e2 = identifier { e = new BinaryNode(",", e, e2); } )* )?
	;

// ********* Class Member Definitions **********
member_definition
	:	field_definition semicolon
	|	method_definition
	|	constructor_definition
	;

field_definition
	:	"field" variable_binding_list[true]
	;

method_definition
    { ExpressionNode e = null; ControlNodeGroup c = new ControlNodeGroup(); }
	:	method_prefix e = identifier function_signature ( block[BlockScope, c] | semicolon)
	;

method_prefix
	:	("final")? ("override")? ("getter" | "setter")? "method"
	;

constructor_definition
    { ExpressionNode e = null; ControlNodeGroup c = new ControlNodeGroup(); }
	:	"constructor" ("new" | e = identifier) parameter_signature block[BlockScope, c]
	;

// ********* Class Definition **********
class_definition
    { ExpressionNode e = null; ControlNodeGroup c = new ControlNodeGroup(); }
	:	"class"
		(
			"extends" type_expression[true, true]
		|	e = identifier ("extends" type_expression[true, true])?
		)
		block[ClassScope, c]
	;

// ********* Programs **********
// Start symbol for JS programs
program returns [ControlNodeGroup contents]
    { contents = new ControlNodeGroup(); }
	:	statements[TopLevelScope, contents]
	;

// ************************************************************************************************
class JSLexer extends Lexer;

// Lexer options
options {
	tokenVocabulary=JS;    // Name the token vocabulary
	testLiterals=false;    // Don't automatically test every token to see if it's a literal.
			       		   // Rather, test only the ones that we explicitly indicate.
	k=4;                   // Set number of lookahead characters
}

// Operators
OPERATORS
	options {testLiterals=true;}
	:	'?'
	|	'('
	|	')'
	|	'['
	|	']'
	|	'{'
	|	'}'
	|	':'
	|	','
//	|	'.' FIXME - conflict w/ NUMBER
	|	'='
	|	"=="
	|	'!'
	|	'~'
	|	"!="
//	|	'/' 	FIXME - conflict w/ REGEXP
//	|	"/="
	|	'+'
	|	"+="
	|	"++"
	|	'-'
	|	"-="
	|	"--"
	|	'*'
	|	"*="
	|	'%'
	|	"%="
	|	">>"
	|	">>="
	|	">>>"
	|	">>>="
	|	">="
	|	">"
	|	"<<"
	|	"<<="
	|	"<="
	|	'<'
	|	'^'
	|	"^="
	|	'|'
	|	"|="
	|	"||"
	|	'&'
	|	"&="
	|	"&&"
	|	";"
//	|	"..."
	;

// Whitespace - mostly ignored
WHITESPACE:
	(	'\t'		// Tab
	|	'\u000B'	// Vertical Tab
	|	'\u000C'	// Form Feed
	|	' '		// Space
	|	LINE_TERMINATOR	// subrule for newlines
	)
	{ $setType(Token.SKIP); }
	;

// Line Terminator
protected
LINE_TERMINATOR:
	(	"\r\n"  // Evil DOS
	|	'\r'    // Macintosh
	|	'\n'    // Unix (the right way)
	)
	{ newline(); }
	;

// Single-line comments
SINGLE_LINE_COMMENT
	:	"//"
		(~('\n'|'\r'))*
		{$setType(Token.SKIP);}
	;

// Multiple-line comments
MULTI_LINE_COMMENT
	:	"/*"
		(	/*	'\r' '\n' can be matched in one alternative or by matching
				'\r' in one iteration and '\n' in another.  I am trying to
				handle any flavor of newline that comes in, but the language
				that allows both "\r\n" and "\r" and "\n" to all be valid
				newline is ambiguous.  Consequently, the resulting grammar
				must be ambiguous.  I'm shutting this warning off.
			 */
			options {
				generateAmbigWarnings=false;
			}
		:
			{ LA(2)!='/' }? '*'
		|	'\r' '\n'		{newline();}
		|	'\r'			{newline();}
		|	'\n'			{newline();}
		|	~('*'|'\n'|'\r')
		)*
		"*/"
		{$setType(Token.SKIP);}
	;

// string literals
STRING
	:	'"' (ESC|~('"'|'\\'))* '"'	// Double-quoted string
	|	'\'' (ESC|~('\''|'\\'))* '\''	// Single-quoted string
	;

// escape sequence -- note that this is protected; it can only be called
//   from another lexer rule -- it will not ever directly return a token to
//   the parser
// There are various ambiguities hushed in this rule.  The optional
// '0'...'9' digit matches should be matched here rather than letting
// them go back to STRING_LITERAL to be matched.  ANTLR does the
// right thing by matching immediately; hence, it's ok to shut off
// the FOLLOW ambig warnings.
protected
ESC
	:	'\\'
		(	'b'
		|	't'
		|	'n'
		|	'f'
		|	'r'
		|	'"'
		|	'\''
		|	'\\'
		|	'u' HEX_DIGIT HEX_DIGIT HEX_DIGIT HEX_DIGIT
		|	('0'..'3')
			(
				options {
					warnWhenFollowAmbig = false;
				}
			:	('0'..'7')
				(
					options {
						warnWhenFollowAmbig = false;
					}
				:	'0'..'7'
				)?
			)?
		|	('4'..'7')
			(
				options {
					warnWhenFollowAmbig = false;
				}
			:	('0'..'7')
			)?
		)
	;


// hexadecimal digit (again, note it's protected!)
protected
HEX_DIGIT
	:	('0'..'9'|'A'..'F'|'a'..'f')
	;

// a numeric literal
NUMBER
	{boolean isDecimal=false;}
	:	'.' {$setType(DOT);}  (('0'..'9')+ (EXPONENT)? { $setType(NUMBER); })?
	|	(	'0' {isDecimal = true;} // special case for just '0'
			(	('x'|'X')
				(											// hex
					// the 'e'|'E' and float suffix stuff look
					// like hex digits, hence the (...)+ doesn't
					// know when to stop: ambig.  ANTLR resolves
					// it correctly by matching immediately.  It
					// is therefor ok to hush warning.
					options {
						warnWhenFollowAmbig=false;
					}
				:	HEX_DIGIT
				)+
			|	('0'..'7')+									// octal
			)?
		|	('1'..'9') ('0'..'9')*  {isDecimal=true;}		// non-zero decimal
		)

		// only check to see if it's a float if looks like decimal so far
		(	{isDecimal}?
				(	'.' ('0'..'9')* (EXPONENT)?
				|	EXPONENT
				)?
			|	// Empty
		)
	;


// A protected methods to assist in matching floating point numbers
protected
EXPONENT
	:	('e'|'E') ('+'|'-')? ('0'..'9')+
	;

// An identifier.  Note that testLiterals is set to true, so that
// after the rule is matched, the literals table is queried to see
// if the token is a literal or if it's really an identifer.
IDENT
	options {testLiterals=true;}
	:	('a'..'z'|'A'..'Z'|'_'|'$') ('a'..'z'|'A'..'Z'|'_'|'0'..'9'|'$')*
	;

// FIXME - this is a placeholder until lexical disambiguation is added between
//         division and regexps.
REGEXP
	: '/' (~('/'|'*'))+ '/'
    ;
