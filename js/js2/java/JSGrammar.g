{
import java.io.*;
// Test program
class Main {
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
	static final int Abbrev = 0;
	static final int AbbrevNonEmpty = 1;
	static final int AbbrevNoShortIf = 2;
	static final int Full = 3;
}

// ********* Identifiers **********
identifier
	:	IDENT
	|	"version"
	|	"override"
	|	"method"
	|	"getter"
	|	"setter"
	|	"traditional"
	|	"constructor"
	;

qualified_identifier
	:	(parenthesized_expression)? ("::" identifier)+
	|	identifier ("::" identifier)*
	;

qualified_identifier_or_parenthesized_expression
	:	(parenthesized_expression | identifier) ("::" identifier)* 
	;

// ********* Primary Expressions **********
primary_expression[boolean initial]
	:	{initial}?
		(
			function_expression
		|	object_literal
		)
	|	simple_expression
	;

simple_expression
	:	"null"
	|	"true"
	|	"false"
	|	NUMBER
	|	STRING
	|	"this"
	|	"super"
	|	qualified_identifier_or_parenthesized_expression
	|	REGEXP
	|	array_literal
	;

parenthesized_expression
	:	"(" expression[false, true] ")"	;

// ********* Function Expressions **********
function_expression
	:	function[false]
	;

// ********* Object Literals **********
object_literal
	:	"{" (field_list)? "}" ;

field_list
	:	literal_field ("," literal_field)* ;

literal_field
	:	qualified_identifier ":" assignment_expression[false, true]
	;

// ********* Array Literals **********
array_literal
	:	"[" (element_list)? "]"	;

element_list
	:	literal_element ("," literal_element)* ;

literal_element
	:	assignment_expression[false, true] ;

// ********* Postfix Unary Operators **********
postfix_expression[boolean initial]
	:
	(	primary_expression[initial]
	|	new_expression
	)
	(
		member_operator
	|	arguments
	|	"++"
	|	"--"
	)*
	;	

new_expression
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

member_operator
	:	"[" argument_list "]"
	|	DOT qualified_identifier_or_parenthesized_expression
	|	"@" qualified_identifier_or_parenthesized_expression
	;

arguments
	:	"(" argument_list ")" ;

argument_list
	:	(assignment_expression[false, true] ("," assignment_expression[false, true])*)? ;

// ********* Prefix Unary Operators **********
unary_expression[boolean initial]
	:	postfix_expression[initial]
	|	"delete" postfix_expression[false]
	|	"typeof" unary_expression[false]
	|	"eval" unary_expression[false]
	| 	"++" postfix_expression[false]
	|	"--" postfix_expression[false]
	|	"+" unary_expression[false]
	|	"-" unary_expression[false]
	|	"~" unary_expression[false]
	|	"!" unary_expression[false]
	;
 
// ********* Multiplicative Operators **********
multiplicative_expression[boolean initial]
	:	unary_expression[initial]
		(("*" | "/" | "%") unary_expression[false])*
	;

// ********* Additive Operators **********
additive_expression[boolean initial]
	:	multiplicative_expression[initial]
		(("+" | "-") multiplicative_expression[false])* 
	;
	
// ********* Bitwise Shift Operators **********
shift_expression[boolean initial]
	:	additive_expression[initial]
		(("<<" | ">>" | ">>>") additive_expression[false])*
	;

// ********* Relational Operators **********
relational_operator[bool allowIn]
	:	{allowIn}? "in"
	|	"<"
	|	">"
	| 	"<="
	| 	">="
	| 	"instanceof"
	;

relational_expression[boolean initial, boolean allowIn]
	:	shift_expression[initial] (relational_operator[allowIn] shift_expression[false])*
	;

// ********* Equality Operators **********
equality_expression[boolean initial, boolean allowIn]
	:	relational_expression[initial, allowIn]
		(("==" | "!=" | "===" | "!==") relational_expression[false, allowIn])*
	;

// ********* Binary Bitwise Operators **********
bitwise_and_expression[boolean initial, boolean allowIn]
	:	equality_expression[initial, allowIn]
		("&" equality_expression[false, allowIn])*
	;

bitwise_xor_expression[boolean initial, boolean allowIn]
	:	bitwise_and_expression[initial, allowIn]
		("^" (bitwise_and_expression[false, allowIn] | "*" | "?"))*
	;

bitwise_or_expression[boolean initial, boolean allowIn]
	:	bitwise_xor_expression[initial, allowIn]
		("|" (bitwise_xor_expression[false, allowIn] | "*" | "?"))*
	;

// ********* Binary Logical Operators **********
logical_and_expression[boolean initial, boolean allowIn]
	:	bitwise_or_expression[initial, allowIn] ("&&" bitwise_or_expression[false, allowIn])*
	;

logical_or_expression[boolean initial, boolean allowIn]
	:	logical_and_expression[initial, allowIn] ("||" logical_and_expression[false, allowIn])*
	;

// ********* Conditional Operators **********
conditional_expression[boolean initial, boolean allowIn]
	:	logical_or_expression[initial, allowIn]
		// There's an ambiguity here:
		//  Consider the input 'a ? b : c ? d : e'.  Is that equivalent to '(a ? b : c) ? d : e' or
		//  should it be interpreted as 'a ? b : (c ? d : e)' ?
		//  Assume the latter by using ANTLR's default behavior of consuming input as
		//  soon as possible and quell the resultant warning.
		(
			options {
           		warnWhenFollowAmbig=false;
            }
		:	"?" assignment_expression[false, allowIn] ":" assignment_expression[false, allowIn]
		)*
	;

non_assignment_expression[boolean initial, boolean allowIn]
	:	logical_or_expression[initial, allowIn]
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
assignment_expression[boolean initial, boolean allowIn]
	:	(postfix_expression[initial] ("=" | compound_assignment)) =>
		postfix_expression[initial] ("=" | compound_assignment) assignment_expression[false, allowIn]
	|	conditional_expression[false, allowIn]
	;

compound_assignment
	:	"*="
	|	"/="
	|	"%="
	|	"+="
	|	"-="
	|	"<<="
	|	">>="
	|	">>>="
	|	"&="
	|	"^="
	|	"|="
	;

// ********* Expressions **********
expression[boolean initial, boolean allowIn]
	:	assignment_expression[initial, allowIn] ("," assignment_expression[false, allowIn])*
	;

optional_expression
	:	(expression[false, true])?
	;

// ********* Type Expressions **********
type_expression[boolean initial, boolean allowIn]
	:	non_assignment_expression[initial, allowIn]
	;

// ********* Statements **********

statement[int scope, int abbrev]
	:	code_statement[abbrev]
//	|	definition[context, abbrev]  FIXME
	;

code_statement[int abbrev]
	:	empty_statement[abbrev]
	|	expression_statement semicolon[abbrev]
	|	block[context]
/*	|	labeled_statement[abbrev]
	|	if_statement[abbrev]
	|	switch_statement[abbrev]
	|	do_statement semicolon[abbrev]
	|	while_statement[abbrev]
	|	for_statement[abbrev]
	|	with_statement[abbrev]
	|	continue_statement semicolon[abbrev]
	|	break_statement semicolon[abbrev]
	|	return_statement semicolon[abbrev]
	|	throw_statement semicolon[abbrev]
	|	try_statement
	|	import_statement[abbrev] FIXME */
	;

semicolon[int abbrev]
// FIXME
//	:	{abbrev == Full}?	";"
//	|	(";")?
	:	";"
	;

// ********* Empty Statement **********
// FIXME
empty_statement[int abbrev]
//	:	{abbrev == Full || abbrev == AbbrevNonEmpty}? ";"
//	|	(";")?
	:	";"
	;

// ********* Expression Statement **********
expression_statement
	:	expression[true, true]
	;

// ********* Block **********
block[int scope]
	:	"{" statements[scope] "}"
	;

// FIXME
statements[int scope]
//	:	statement[scope, Abbrev]
//	|	(statement[scope, Full])+ statement[scope, AbbrevNonEmpty]
	:	(statement[scope, Full])+
	;

// ********* Labeled Statements **********
labeled_statement[int abbrev]
	:	identifier ":" code_statement[abbrev]
	;

// ********* If statement **********
if_statement[int abbrev]
	:	"if" 
	;

// ********* Switch statement **********
switch_statement
	:	"switch" parenthesized_expression "{" (case_groups last_case_group)? "}"
	;

case_groups
	:	(case_group)+
	;

case_group
	:	(case_guard)+ (code_statement[full])+
	;

last_case_group
	:	(case_guard)+ case_statements
	;

case_guard
	:	"case" expression[false, true] ":"
	|	"default" ":"
	;

// FIXME
case_statements
//	:	(code_statement[Full])+ code_statement[AbbrevNonEmpty]
	:	(code_statement[Full])+
	;

// ********* Do-While statement **********
do_statement
	:	"do" code_statement[AbbrevNonEmpty] "while" parenthesized_expression
	;

// ********* While statement **********
while_statement[int abbrev]
	:	"while" parenthesized_expression code_statement[abbrev]
	;

// ********* For statement **********
for_statement[int abbrev]
	:	"for" "("
		(
			(for_initializer ";") => for_initializer ";" optional_expression ";" optional_expression
		|	for_in_binding "in" expression[false, true]
		)
		")"
		code_statement[abbrev]
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
with_statement[int abbrev]
	:	"with" parenthesized_expression code_statement[abbrev]
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

function[boolean nameRequired]
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
	:	(type_expression[true, true])?
	;

traditional_function
	: "traditional" "function" identifier "(" traditional_parameter_list ")" block[BlockScope]
	;

traditional_parameter_list
	:	(identifier ("," identifier)* )?
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
