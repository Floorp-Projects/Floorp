#!/usr/local/bin/perl
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.
#

# PrepareBurg.pl
#
# Assembles a BURG rule file from different inputs
#
# Usage:
#   perl  PrepareBurg.pl 
#		<PrimitiveOperations> 					input				primitive operations description
#			< <MachineSpecificRulesFileName>	input	(stdin)		machine specific BURG style rules
#			> <BURG OutputFile>					output	(stdout)
#

use PrimOp;

# rules in BURG must be >= 1
# terminals in BURG must be >= 1

# grab the arguments
($PrimitiveOperationsSourceFN) = @ARGV;

PrimOp::readPrimitiveOperations($PrimitiveOperationsSourceFN);

# cruft at top
print "%{\n";
print q!#include "Burg.h"!;
print "\n%}\n";

# start symbol
# we use a false one so we can have multiple start symbols
print "%start xxxGeneratedStartSymbol\n";

# **** BEGIN %term section

# input is gPrimitiveInfo
# output is @ts, a list of terminals and there numbers (assumed to start
# at 1 (skipping poNone) and continuing on.
# convert a primitive info into a list of terminal names
# skip first terminal which is poNone (because we should never see that primitive
# and BURG can't handle terminals <= 0
$gTermNo = 1;
foreach $x (@PrimOp::gPrimitiveInfo[$gTermNo..$#PrimOp::gPrimitiveInfo]) {
	if ($x->[$nameIndex] ne "") {	
   		print "%term $x->[$nameIndex] = $gTermNo\n";
		$gTermNo++;
	}
}

# **** BEGIN rules section
print "%%\n";

# Open the file containing the BurgRules and just print them 
# to stdout
$gCurRule = 1;
while (<STDIN>) {
	$gMachineSpecificRules[$gCurRule++] = $_;
}

print 	"Vint:		coReg_I			 	= ", $gCurRule++, " (0);\n",
		"Vlong:		coReg_L			 	= ", $gCurRule++, " (0);\n",
		"Vfloat:	coReg_F		 	 	= ", $gCurRule++, " (0);\n",
		"Vdouble:	coReg_D			 	= ", $gCurRule++, " (0);\n",
		"Vptr:		coReg_A			 	= ", $gCurRule++, " (0);\n",
		"Vcond:		coReg_C			 	= ", $gCurRule++, " (0);\n",
		"Store:		coReg_M			 	= ", $gCurRule++, " (0);\n",
		"Cint:		coReg_I			 	= ", $gCurRule++, " (0);\n",
		"Clong:		coReg_L			 	= ", $gCurRule++, " (0);\n",
		"Cfloat:	coReg_F			 	= ", $gCurRule++, " (0);\n",
		"Cdouble:	coReg_D			 	= ", $gCurRule++, " (0);\n",
		"Cptr:		coReg_A			 	= ", $gCurRule++, " (0);\n",	
		"Tuple:		coReg_T		 		= ", $gCurRule++, " (0);\n",		
		"Vint:		poArg_I			 	= ", $gCurRule++, " (0);\n",			
		"Vlong:		poArg_L			 	= ", $gCurRule++, " (0);\n",			
		"Vfloat:	poArg_F			 	= ", $gCurRule++, " (0);\n",			
		"Vdouble:	poArg_D			 	= ", $gCurRule++, " (0);\n",				
		"Vptr:		poArg_A			 	= ", $gCurRule++, " (0);\n",				
		"Store:		poArg_M			 	= ", $gCurRule++, " (0);\n",				
		"Result:	poResult_M(Store)	= ", $gCurRule++, " (0);\n",			
		"Store:		poProj_M		 	= ", $gCurRule++, " (0);\n",		
		"Vint:		poProj_I		 	= ", $gCurRule++, " (0);\n",		
		"Vptr:		poProj_A		 	= ", $gCurRule++, " (0);\n",		
		"Vptr:		poConst_M		 	= ", $gCurRule++, " (0);\n";		

# now print out the start symbols
# start symbols are anonymous rules
@gStartSymbols = qw(Control Result Exception Store Vcond Vint Vlong Vfloat Vdouble Vptr Cint Clong Cfloat Cdouble Cptr Tuple);
foreach $x (@gStartSymbols) {
    print "xxxGeneratedStartSymbol:\t$x \t = $gCurRule (0);\n";
    $gCurRule++;
}

# now print out those rules
$gCurRule = 1;
foreach $x (@gMachineSpecificRules[$gCurRule..$#gMachineSpecificRules]) {
	print $x;
	$gCurRule++;
}

# end grammar section
print "%%\n";

1;
