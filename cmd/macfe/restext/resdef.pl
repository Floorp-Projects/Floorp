#! /usr/local/bin/perl

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

# This script reads a series of lines of the form:
# resource 'STR ' ( id+100, "name", purgeable)(NAME, ID, "STRING")
# and generates stuff like:

open(OUT, ">xpstring.r");
print OUT "// This is a generated file, do not edit it\n";
print OUT "#include \"Types.r\"\n";
while (<>) {
  ($enum, $eid) =
    /[ 	]*([A-Za-z0-9_]+)[ 	]*\=[ 	]*([A-Za-z_x0-9]+),/;
	 if ($enum ne "") {
    print OUT "#define $enum $eid \n";
  }
  ($name, $id, $string) =
    /ResDef[ 	]*\([ 	]*([A-Za-z0-9_]+)[ 	]*,[ 	]*([\(\)0-9A-Za-z_x\-+ ]+)[ 	\01]*,[ 	]*(".*")[ 	]*\)/;
	 if ($name ne "") {
#   print OUT "resource 'STR ' (($id)+7000, \"$name\", purgeable)\n{\n\t";
    print OUT "resource 'STR ' (($id)+7000, \"\", purgeable)\n{\n\t";

	$_ = $string;
	s/([^.:])\\n/$1 /g;
	s/(\\n) /$1/g;
	print OUT "$_";
	
	print OUT ";\n};\n";
  }
}
close(OUT);


