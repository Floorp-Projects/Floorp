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
# MakePrimOp.pl
#
# Scott M. Silver
#
# Parses PrimitiveOperations file.
#

use PrimOp;

# Usage:
#   perl  PrepareBurg.pl 
#		<PrimitiveOperations> 					input				primitive operations description
#		<PrimitiveOperations Enum FN> 			output
#		<PrimitiveOperations FN> 				output

($PrimitiveOperationsSourceFN, $PrimitiveOperationsEnumFN, $DataNodeTemplateFN) = @ARGV;

PrimOp::readPrimitiveOperations($PrimitiveOperationsSourceFN);
PrimOp::createPrimitiveOperationsCpp($DataNodeTemplateFN);
PrimOp::createPrimitiveOperationsH($PrimitiveOperationsEnumFN);
