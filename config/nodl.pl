#! perl
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

# 
# Print out the nodltab.
# Usage: nodl.pl table-name sym1 sym2 ... symN
# 

$table = $ARGV[0];
shift(@ARGV);

print "/* Automatically generated file; do not edit */\n\n";

print "#include \"prtypes.h\"\n\n";
print "#include \"prlink.h\"\n\n";

foreach $symbol (@ARGV) {
    print "extern void ",$symbol,"();\n";
}
print "\n";

print "PRStaticLinkTable ",$table,"[] = {\n";
foreach $symbol (@ARGV) {
    print "  { \"",$symbol,"\", ",$symbol," },\n";
}
print "  { 0, 0, },\n";
print "};\n";
