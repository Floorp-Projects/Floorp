#!/usr/sbin/perl
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
# addnpl.pl
#
# This script adds the NPL header comment to every source file in the
# command line argument that ends in .c, .cpp, .h, .rc, .def,
# .script, .s, .mak, and .bat.
# If no extension, then adds Makefile like comments.
# NOTE: Don't use this script on perl script files since it won't correct for
# the first line path to perl executable.
#
# For example:
# > perl addnpl.pl test1.c test2.cpp test3.mak
#
# would add the C-style NPL License comment to test1.c and test2.cpp. and
# the makefile-style NPL License comment to test3.mak
    
if (0 > $#ARGV)
{
    print STDERR "You need to supply a list of test files on the cmd line\n";
    die;
}

sub licenseHeader
{
    $cFile = 0;
    $startToken = "#";
    local($fileExtension) = @_;
    if ($fileExtension =~ /(h|c|cpp|rc)/) {
	$startToken = " *";
	$cFile = 1;
    }
    elsif ($fileExtension =~ /(mak|s|script)/) {
	$startToken = "#";
    }
    elsif ($fileExtension =~ /def/) {
	$startToken = ";";
    }
    elsif ($fileExtension =~ /bat/) {
	$startToken = "rem";
	$cFile = 2;
    }
    else {
	$startToken = "#";
    }
    if ($cFile == 1) {
	print OUT "/* -\*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset:2 -\*- \n";
	print OUT "$startToken \n";
    }
    elsif ($cFile == 2) {
	print OUT "@echo off \n";
    }
    print OUT "$startToken The contents of this file are subject to the Netscape Public License \n"; 
    print OUT "$startToken Version 1.0 (the \"NPL\"); you may not use this file except in \n";
    print OUT "$startToken compliance with the NPL.  You may obtain a copy of the NPL at \n";
    print OUT "$startToken http://www.mozilla.org/NPL/ \n"; 
    print OUT "$startToken \n"; 
    print OUT "$startToken Software distributed under the NPL is distributed on an \"AS IS\" basis, \n"; 
    print OUT "$startToken WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL \n"; 
    print OUT "$startToken for the specific language governing rights and limitations under the \n"; 
    print OUT "$startToken NPL. \n"; 
    print OUT "$startToken \n";
    print OUT "$startToken The Initial Developer of this code under the NPL is Netscape \n";
    print OUT "$startToken Communications Corporation.  Portions created by Netscape are \n";
    print OUT "$startToken Copyright (C) 1998 Netscape Communications Corporation.  All Rights \n";
    print OUT "$startToken Reserved. \n";
    if ($cFile == 1)
    {
	print OUT " */ \n";
    }
    elsif ($cFile == 2)
    {
	print OUT "@echo on \n";
    }
    print OUT "\n";
}

foreach $i (0..$#ARGV)
{
    $j = 0;
    $size = 0;
    
    $filename = $ARGV[$i];
    if (!open(NPLFILE, "+<$filename"))
    {
	print STDERR "Can't open $filename \n";
	print STDERR "error: $!\n";
	die;
    }

    $extension = substr($filename, 1+index($filename, "."));
    print STDERR "Adding NPL license header to $filename\n";

    while ($line = <NPLFILE>)
    {
	$buffer[$j++] = $line;
	$size++;
    }

    close NPLFILE;
    
    open(OUT, "+>$filename");
    
    &licenseHeader($extension);
    $j = 0;
    while ($j < $size)
    {
	print OUT $buffer[$j];
	$j++;
    }
    close OUT;

}










