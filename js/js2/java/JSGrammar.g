{
import java.io.*;
// Test program
class TestMain {
	public static void main(String[] args) {
		try {
			JSLexer lexer = new JSLexer(new DataInputStream(System.in));
			JSParser parser = new JSParser(lexer);
			parser.expression(true, true);
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

	// Possible abbreviation contexts
/*	static final int Abbrev = 0;
	static final int AbbrevNonEmpty = 1;
	static final int AbbrevNoShortIf = 2;
	static final int Full = 3;*/
}

// ********* Identifiers **********
identifier returns [ExpressionNode e]
    { e = null; }
	:	opI:IDENT       { e = new JSValue("object", opI.getText()); }
	|	"version"       { e = new JSValue("object", "version"); }
	|	"override"      { e = new JSValue("object", "override"); }
	|	"method"        { e = new JSValue("object", "method"); }
	|	"getter"        { e = new JSValue("object", "getter"); }
	|	"setter"        { e = new JSValue("object", "setter"); }
	|	"traditional"   { e = new JSValue("object", "traditional"); }
	|	"constructor"   { e = new JSValue("object", "constructor"); }
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
	:	{initial}?
		(
			e = function_expression
		|	e = object_literal
		)
	|	e = simple_expression
	;

simple_expression returns [ExpressionNode e]
    { e = null; }
	:	"null"      { e = new JSValue("object", "null"); }
	|	"true"      { e = new JSValue("boolean", "true"); }
	|	"false"     { e = new JSValue("boolean", "false"); }
	|	opN:NUMBER  { e = new JSValue("number", opN.getText()); }
	|	opS:STRING  { e = new JSValue("number", opS.getText()); }
	|	"this"      { e = new JSValue("object", "this"); }
	|	"super"     { e = new JSValue("object", "super"); }
	|	e = qualified_identifier_or_parenthesized_expression
	|	REGEXP
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
	:	"{" (field_list)? "}" ;

field_list returns [ExpressionNode e]
    { e = null; }
	:	literal_field ("," literal_field)* ;

literal_field returns [ExpressionNode e]
    { e = null; }
	:	qualified_identifier ":" assignment_expression[false, true]
	;

// ********* Array Literals **********
array_literal returns [ExpressionNode e]
    { e = null; }
	:	"[" (element_list)? "]"	;

element_list
	:	literal_element ("," literal_element)* ;

literal_element
	:	assignment_expression[false, true] ;

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
			new_expression
		|	(
				primary_expression[false]
				(
					// There's an ambiguity here:
					//  Consider the input 'new f.x'.  Is that equivalent to '(new f).x' or 'new (f.x)' ?
					//  Assume the latter by using ANTLR's default behavior of consuming input as
					//  soon as possible and quell the resultant warning.
					options {
                    	warnWhenFollowAmbig=false;
                    }
				:	member_operator
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
		:	arguments
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
	:	"(" argument_list ")" ;

argument_list returns [ExpressionNode e]
    { e = null; }
	:	(assignment_expression[false, true] ("," assignment_expression[false, true])*)? ;

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
	    ("<<" r = additive_expression[false] { e = new ArithmeticNode("<<", e, r); } )
	|	(">>" r = additive_expression[false] { e = new ArithmeticNode(">>", e, r); } )
	|	(">>>" r = additive_expression[false] { e = new ArithmeticNode(">>>", e, r); } )
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
		("&" r = equality_expression[false, allowIn] { e = new ArithmeticNode("&", e, r); } )*
	;

bitwise_xor_expression[boolean initial, boolean allowIn] returns [ExpressionNode e]
    { e = null; ExpressionNode r = null; }
	:	e = bitwise_and_expression[initial, allowIn]
		("^" (r = bitwise_and_expression[false, allowIn] { e = new BinaryNode("^", e, r); } | "*" | "?"))*
	;

bitwise_or_expression[boolean initial, boolean allowIn] returns [ExpressionNode e]
    { e = null; ExpressionNode r = null; }
	:	e = bitwise_xor_expression[initial, allowIn]
		("|" (r = bitwise_xor_expression[false, allowIn] { e = new BinaryNode("|", e, r); } | "*" | "?"))*
	;

// ********* Binary Logical Operators **********
logical_and_expression[boolean initial, boolean allowIn] returns [ExpressionNode e]
    { e = null; ExpressionNode r = null; }
	:	e = bitwise_or_expression[initial, allowIn]
	    ("&&" r = bitwise_or_expression[false, allowIn] { e = new BinaryNode("&&", e, r); } )*
	;

logical_or_expression[boolean initial, boolean allowIn] returns [ExpressionNode e]
    { e = null; ExpressionNode r = null; }
	:	e = logical_and_expression[initial, allowIn]
	    ("||" r = logical_and_expression[false, allowIn] { e = new BinaryNode("||", e, r); } )*
	;

// ********* Conditional Operators **********
conditional_expression[boolean initial, boolean allowIn] returns [ExpressionNode e]
    { e = null; }
	:	e = logical_or_expression[initial, allowIn]
		// There's an ambiguity here:
		//  Consider the input 'a ? b : c ? d : e'.  Is that equivalent to '(a ? b : c) ? d : e' or
		//  should it be interpreted as 'a ? b : (c ? d : e)' ?
		//  Assume the latter by using ANTLR's default behavior of consuming input as
		//  soon as possible and quell the resultant warning.
		(
			options {warnWhenFollowAmbig=false;}
		:	"?" assignment_expression[false, allowIn] ":" assignment_expression[false, allowIn]
		)*
	;

non_assignment_expression[boolean initial, boolean allowIn] returns [ExpressionNode e]
    { e = null; }
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
		:	"?" non_assignment_expression[false, allowIn] ":" non_assignment_expression[false, allowIn]
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

optional_expression
	:	(expression[false, true])?
	;

// ********* Type Expressions **********
type_expression[boolean initial, boolean allowIn]
	:	non_assignment_expression[initial, allowIn]
	;

// ********* Statements **********

statement[int scope, boolean non_empty] returns [ControlNode c]
    { c = null; }
	:	c = code_statement[non_empty]
	|	definition[scope]
	;

code_statement[boolean non_empty] returns [ControlNode c]
    { c = null; }
	:	empty_statement[non_empty]
	|	(identifier ":") => labeled_statement[non_empty]
	|	c = expression_statement semicolon
	|	block[BlockScope]
	|	c = if_statement[non_empty]
	|	switch_statement
	|	do_statement semicolon
	|	while_statement[non_empty]
	|	for_statement[non_empty]
	|	with_statement[non_empty]
	|	continue_statement semicolon
	|	break_statement semicolon
	|	return_statement semicolon
	|	throw_statement semicolon
	|	try_statement
	|	import_statement[non_empty]
	;

semicolon
// FIXME - Add abbreviation
	:	";"
	;

// ********* Empty Statement **********
// FIXME
empty_statement[boolean non_empty]returns [ControlNode c]
    { c = null; }
	:	";"
	;

// ********* Expression Statement **********
expression_statement returns [ControlNode c]
    { c = null; ExpressionNode e = null; }
	:	e = expression[true, true] { c = new ControlNode(e); }
	;

// ********* Block **********
block[int scope] returns [ControlNode c]
    { c = null; }
	:	"{" c = statements[scope] "}"
	;

// FIXME
statements[int scope] returns [ControlNode c]
    { c = null; ControlNode t = null; ControlNode n = null; }
	:	 c = statement[scope, false] { t = c; }
	        ( n = statement[scope, false] { t.setNext(n); t = n; } )*
	;

// ********* Labeled Statements **********
labeled_statement[boolean non_empty]
	:	identifier ":" code_statement[non_empty]
	;

if_statement[boolean non_empty] returns [ConditionalNode c]
    { c = null; ControlNode t = null; ControlNode f = null; ExpressionNode e = null; }
	:	"if"  e = parenthesized_expression t = code_statement[non_empty] { c = new ConditionalNode(e, t); }
		(
			// Standard if/else ambiguity
			options { warnWhenFollowAmbig=false; }:
			"else" t = code_statement[non_empty] { c.setFalse(t); }
		)?
	;

// ********* Switch statement **********
switch_statement
	:	"switch" parenthesized_expression "{" (case_groups)? "}"
	;

case_groups
	:	(case_group)+
	;

case_group
	:	(case_guard)+ (code_statement[true])+
	;

case_guard
	:	"case" expression[false, true] ":"
	|	"default" ":"
	;

// FIXME
case_statements
	:	(code_statement[false])+
	;

// ********* Do-While statement **********
do_statement
	:	"do" code_statement[true] "while" parenthesized_expression
	;

// ********* While statement **********
while_statement[boolean non_empty]
	:	"while" parenthesized_expression code_statement[non_empty]
	;

// ********* For statement **********
for_statement[boolean non_empty]
	:	"for" "("
		(
			(for_initializer ";") => for_initializer ";" optional_expression ";" optional_expression
		|	for_in_binding "in" expression[false, true]
		)
		")"
		code_statement[non_empty]
	;

for_initializer
	:	(
			expression[false, true]
		|	("var" | "const") variable_binding_list[false]
		)?
	;

for_in_binding
	:	(
			postfix_expression[false]
		|	("var" | "const") variable_binding_list[false]
		)
	;

// ********* With statement **********
with_statement[boolean non_empty]
	:	"with" parenthesized_expression code_statement[non_empty]
	;

// ********* Continue and Break statement **********
continue_statement
	:	"continue" (identifier)?
	;

break_statement
	:	"break" (identifier)?
	;

// ********* Return statement **********
return_statement
	:	"return" optional_expression
	;

// ********* Throw statement **********
throw_statement
	:	"throw" expression[false, true]
	;

// ********* Try statement **********
try_statement
	:	"try"	block[BlockScope] (catch_clause)* ("finally" block[BlockScope])?
	;

catch_clause
	:	"catch"	"(" typed_identifier[true] ")" block[BlockScope]
	;

// ********* Import statement **********
import_statement[boolean non_empty]
	:	"import" import_list
		(
			";"
		|	block[BlockScope] ("else" code_statement[non_empty])
		)
	;

import_list
	:	import_item ("," import_item)*
	;

import_item
	:	(identifier "=") => identifier "=" import_source
	|	import_source
	|	"protected" identifier "=" import_source
	;

import_source
	:	non_assignment_expression[false, false] (":" Version)
	;

// ********* Definitions **********
definition[int scope]
	:	visibility global_definition
	|	local_definition[scope]
	;

global_definition
	:	version_definition semicolon
	|	variable_definition semicolon
	|	function_definition
	|	member_definition
	|	class_definition
	;

local_definition[int scope]
	:	{scope == TopLevelScope || scope == ClassScope}? (class_definition | member_definition)
	|	variable_definition semicolon
	|	function_definition
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
	:	version_range (":" identifier)
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
	:	typed_identifier[allowIn] ("=" assignment_expression[false, allowIn])?
	;

typed_identifier[boolean allowIn]
	:	(type_expression[false, allowIn] identifier) => type_expression[false, allowIn] identifier
	|	identifier
	;

// ********* Function Definition **********
function_definition
	:	named_function
	|	"getter" named_function
	|	"setter" named_function
	|	traditional_function
	;

anonymous_function
	:	function[false]
	;

named_function
	:	function[true]
	;

function[boolean nameRequired] returns [ExpressionNode e]
    { e = null; }
	:	"function"
		(
			{nameRequired}? identifier
		|	identifier
		)
		function_signature block[BlockScope]
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
	:	typed_identifier[true] ("=" (assignment_expression[false, true])? )?
	;

rest_parameter
	:	"..." (identifier)?
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
	: "traditional" "function" identifier "(" traditional_parameter_list ")" block[BlockScope]
	;

traditional_parameter_list
	:	(identifier ("," identifier)* )?
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
	:	method_prefix identifier function_signature (block[BlockScope] | semicolon)
	;

method_prefix
	:	("final")? ("override")? ("getter" | "setter")? "method"
	;

constructor_definition
	:	"constructor" ("new" | identifier) parameter_signature block[BlockScope]
	;

// ********* Class Definition **********
class_definition
	:	"class"
		(
			"extends" type_expression[true, true]
		|	identifier ("extends" type_expression[true, true])?
		)
		block[ClassScope]
	;

// ********* Programs **********
// Start symbol for JS programs
program
	:	statements[TopLevelScope]
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
