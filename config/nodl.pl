#! perl
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
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
