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

/**
 * Defines token classes and their print names.
 *
 * This interface defines values for each token class that occurs in
 * ECMAScript 4. All but numericliteral, stringliteral, regexpliteral, and
 * identifier are singleton and therefore a fully described by their
 * token class defined here. The other four however can have an infinite number
 * of instances each with a unique identifier and associated instance
 * data. We use positive identifiers to signify instances of these 
 * token classes so that the instance data can be stored in an array,
 * or set of arrays with the token value specifying its index.
 */

public interface Tokens {

	/**
	 * Token class values are negative, and token instances are positive so
	 * that their values can point to their instance data in an array.
	 */

	public static final int first_token                 = -1;
	public static final int eos_token                   = first_token-0;

	public static final int minus_token					= first_token - 1;
	public static final int minusminus_token            = minus_token - 1;					
	public static final int not_token					= minusminus_token - 1;
	public static final int notequals_token				= not_token - 1;
	public static final int strictnotequals_token		= notequals_token - 1;
	public static final int pound_token		            = strictnotequals_token - 1;
	public static final int modulus_token				= pound_token - 1;
	public static final int modulusassign_token			= modulus_token - 1;
	public static final int bitwiseand_token			= modulusassign_token - 1;
	public static final int logicaland_token			= bitwiseand_token - 1;
	public static final int logicalandassign_token		= logicaland_token - 1;
	public static final int bitwiseandassign_token		= logicalandassign_token - 1;
	public static final int leftparen_token				= bitwiseandassign_token - 1;
	public static final int rightparen_token			= leftparen_token - 1;
	public static final int mult_token					= rightparen_token - 1;
	public static final int multassign_token			= mult_token - 1;
	public static final int comma_token					= multassign_token - 1;	
	public static final int dot_token					= comma_token - 1;	
	public static final int doubledot_token				= dot_token - 1;	
	public static final int tripledot_token				= doubledot_token - 1;	
	public static final int div_token					= tripledot_token - 1;	
	public static final int divassign_token				= div_token - 1;	
	public static final int colon_token					= divassign_token - 1;	
	public static final int doublecolon_token			= colon_token - 1;	
	public static final int semicolon_token				= doublecolon_token - 1;	
	public static final int questionmark_token			= semicolon_token - 1;	
	public static final int ampersand_token			    = questionmark_token - 1;	
	public static final int leftbracket_token			= ampersand_token - 1;	
	public static final int rightbracket_token			= leftbracket_token - 1	;
	public static final int bitwisexor_token			= rightbracket_token - 	1;
	public static final int logicalxor_token			= bitwisexor_token - 1;	
	public static final int logicalxorassign_token		= logicalxor_token - 1;	
	public static final int bitwisexorassign_token		= logicalxorassign_token - 1;	
	public static final int leftbrace_token				= bitwisexorassign_token - 1;
	public static final int bitwiseor_token				= leftbrace_token - 1;	
	public static final int logicalor_token				= bitwiseor_token - 1;	
	public static final int logicalorassign_token		= logicalor_token - 1;	
	public static final int bitwiseorassign_token		= logicalorassign_token - 1;	
	public static final int rightbrace_token			= bitwiseorassign_token - 1;
  	public static final int bitwisenot_token			= rightbrace_token - 1;
  	public static final int plus_token					= bitwisenot_token - 1;
  	public static final int plusplus_token				= plus_token - 1;
  	public static final int plusassign_token			= plusplus_token - 1;
  	public static final int lessthan_token				= plusassign_token - 1;
  	public static final int leftshift_token				= lessthan_token - 1;
  	public static final int leftshiftassign_token		= leftshift_token - 1;
  	public static final int lessthanorequals_token		= leftshiftassign_token - 1;
  	public static final int assign_token				= lessthanorequals_token - 1;
  	public static final int minusassign_token			= assign_token - 1;
  	public static final int equals_token				= minusassign_token - 1;
  	public static final int strictequals_token			= equals_token - 1;
  	public static final int greaterthan_token			= strictequals_token - 1;
  	public static final int arrow_token			        = greaterthan_token - 1;
  	public static final int greaterthanorequals_token	= arrow_token - 1;
  	public static final int rightshift_token			= greaterthanorequals_token - 1;
  	public static final int rightshiftassign_token		= rightshift_token - 1;
  	public static final int unsignedrightshift_token	= rightshiftassign_token - 1;
  	public static final int unsignedrightshiftassign_token	= unsignedrightshift_token - 1;
  	public static final int abstract_token				= unsignedrightshiftassign_token - 1;
  	public static final int attribute_token				= abstract_token - 1;
  	public static final int boolean_token				= attribute_token - 1;
  	public static final int break_token					= boolean_token - 1;
  	public static final int byte_token					= break_token - 1;
  	public static final int case_token					= byte_token - 1;
  	public static final int catch_token					= case_token - 1;
  	public static final int char_token					= catch_token - 1;
  	public static final int class_token					= char_token - 1;
  	public static final int const_token					= class_token - 1;
  	public static final int constructor_token			= const_token - 1;
  	public static final int continue_token				= constructor_token - 1;
  	public static final int debugger_token				= continue_token - 1;
  	public static final int default_token				= debugger_token - 1;
  	public static final int delete_token				= default_token - 1;
  	public static final int do_token					= delete_token - 1;
  	public static final int double_token				= do_token - 1;
  	public static final int else_token					= double_token - 1;
  	public static final int enum_token					= else_token - 1;
  	public static final int eval_token					= enum_token - 1;
  	public static final int export_token				= eval_token - 1;
  	public static final int extends_token				= export_token - 1;
  	public static final int false_token					= extends_token - 1;
  	public static final int final_token					= false_token - 1;
  	public static final int finally_token				= final_token - 1;
  	public static final int float_token					= finally_token - 1;
  	public static final int for_token					= float_token - 1;
  	public static final int function_token				= for_token - 1;
  	public static final int get_token				    = function_token - 1;
  	public static final int goto_token					= get_token - 1;
  	public static final int if_token					= goto_token - 1;
  	public static final int implements_token			= if_token - 1;
  	public static final int import_token				= implements_token - 1;
  	public static final int in_token					= import_token - 1;
  	public static final int include_token				= in_token - 1;
  	public static final int instanceof_token			= include_token - 1;
  	public static final int int_token					= instanceof_token - 1;
  	public static final int interface_token				= int_token - 1;
  	public static final int long_token					= interface_token - 1;
  	public static final int namespace_token				= long_token - 1;
  	public static final int native_token				= namespace_token - 1;
  	public static final int new_token					= native_token - 1;
  	public static final int null_token					= new_token - 1;
  	public static final int package_token				= null_token - 1;
  	public static final int private_token				= package_token - 1;
  	public static final int protected_token				= private_token - 1;
  	public static final int public_token				= protected_token - 1;
  	public static final int return_token				= public_token - 1;
  	public static final int set_token				    = return_token - 1;
  	public static final int short_token					= set_token - 1;
  	public static final int static_token				= short_token - 1;
  	public static final int super_token					= static_token - 1;
  	public static final int switch_token				= super_token - 1;
  	public static final int synchronized_token			= switch_token - 1;
  	public static final int this_token					= synchronized_token - 1;
  	public static final int throw_token					= this_token - 1;
  	public static final int throws_token				= throw_token - 1;
  	public static final int transient_token				= throws_token - 1;
  	public static final int true_token					= transient_token - 1;
  	public static final int try_token					= true_token - 1;
  	public static final int typeof_token				= try_token - 1;
  	public static final int use_token				    = typeof_token - 1;
  	public static final int var_token					= use_token - 1;
  	public static final int void_token					= var_token - 1;
  	public static final int volatile_token				= void_token - 1;
  	public static final int while_token					= volatile_token - 1;
  	public static final int with_token					= while_token - 1;
  	public static final int identifier_token			= with_token - 1;
  	public static final int numberliteral_token			= identifier_token - 1;
  	public static final int regexpliteral_token			= numberliteral_token - 1;
  	public static final int stringliteral_token			= regexpliteral_token - 1;
	
    public static final int eol_token                   = stringliteral_token - 1;

    public static final int empty_token                 = eol_token - 1;
    public static final int error_token                 = empty_token - 1;
    public static final int last_token                  = empty_token - 1;

	public static final String[] tokenClassNames = {
		"<unused index>", 
		"<eos>",
		"minus_token",
		"minusminus_token",
		"not_token",
		"notequals_token",
		"strictnotequals_token",
		"pound_token",
		"modulus_token",
		"modulusassign_token",
		"bitwiseand_token",
		"logicaland_token",
		"logicalandassign_token",
		"bitwiseandassign_token",
		"leftparen_token",
		"rightparen_token",
		"mult_token",
		"multassign_token",
		"comma_token",
		"dot_token",
		"doubledot_token",
		"tripledot_token",
		"div_token",
		"divassign_token",
		"colon_token",
		"doublecolon_token",
		"semicolon_token",
		"questionmark_token",
		"ampersand_token",
		"leftbracket_token",
		"rightbracket_token",
		"bitwisexor_token",
		"logicalxor_token",
		"logicalxorassign_token",
		"bitwisexorassign_token",
		"leftbrace_token",
		"bitwiseor_token",
		"logicalor_token",
		"logicalorassign_token",
		"bitwiseorassign_token",
		"rightbrace_token",
		"bitwisenot_token",
		"plus_token",
		"plusplus_token",
		"plusassign_token",
		"lessthan_token",
		"leftshift_token",
		"leftshiftassign_token",
		"lessthanorequals_token",
		"assign_token",
		"minusassign_token",
		"equals_token",
		"strictequals_token",
		"greaterthan_token",
		"arrow_token",
		"greaterthanorequals_token",
		"rightshift_token",
		"rightshiftassign_token",
		"unsignedrightshift_token",
		"unsignedrightshiftassign_token",
		"abstract_token",
		"attribute_token",
		"boolean_token",
		"break_token",
		"byte_token",
		"case_token",
		"catch_token",
		"char_token",
		"class_token",
		"const_token",
		"constructor_token",
		"continue_token",
		"debugger_token",
		"default_token",
		"delete_token",
		"do_token",
		"double_token",
		"else_token",
		"enum_token",
		"eval_token",
		"export_token",
		"extends_token",
		"false_token",
		"final_token",
		"finally_token",
		"float_token",
		"for_token",
		"function_token",
		"get_token",
		"goto_token",
		"if_token",
		"implements_token",
		"import_token",
		"in_token",
		"include_token",
		"instanceof_token",
		"int_token",
		"interface_token",
		"long_token",
		"namespace_token",
		"native_token",
		"new_token",
		"null_token",
		"package_token",
		"private_token",
		"protected_token",
		"public_token",
		"return_token",
		"set_token",
		"short_token",
		"static_token",
		"super_token",
		"switch_token",
		"synchronized_token",
		"this_token",
		"throw_token",
		"throws_token",
		"transient_token",
		"true_token",
		"try_token",
		"typeof_token",
		"use_token",
		"var_token",
		"void_token",
		"volatile_token",
		"while_token",
		"with_token",
		"identifier_token",
		"numberliteral_token",
		"regexpliteral_token",
		"stringliteral_token",

		"<eol>",
		"<empty>",
		"<error>"
	};
}

/*
 * The end.
 */
