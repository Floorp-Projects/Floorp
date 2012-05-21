#! perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# Converts a list of atoms in the form:
# // OUTPUT_CLASS=<classname>
# // MACRO_NAME=<macro>
# <macroname>(atomName, "String")
# <macroname>(atomName2, "String2")
#
# into a file suitable for gperf using static atoms
#
# usage:
# make-atom-strings < file.h > file.gperf
#
# the lines in the C++ comments define two variables:
# OUTPUT_CLASS is the class who has all the atoms as members
# MACRO_NAME is the macro to look for in the rest of the file
#
# for example
# // OUTPUT_CLASS=nsHTMLAtoms
# // MACRO_NAME=HTML_ATOM
# HTML_ATOM(a, "a")
# HTML_ATOM(body, "body")
#
# etc...
#
# this will generate a file that looks like:
# struct nsStaticAtom ( const char* mValue; nsIAtom** aAtom; }
# %%
# "a", &nsHTMLAtoms::a
# "body", &nsHTMLAtoms::body
#
# etc...
#
# the output can be plugged into gperf to generate a perfect hash

print "struct nsStaticAtom {const char* mValue; nsIAtom** aAtom; };\n";
print "%%\n";

my $classname, $macroname;

while (<>) {
  chop;
  if (/OUTPUT_CLASS=(\S+)/) {
    $classname=$1;
  } elsif (/MACRO_NAME=(\S+)/) {
    $macroname=$1;
  }
  elsif ($classname && $macroname &&
         /$macroname\((\S+),\s*\"(.*?)\"\s*\)/) {
    my ($str, $atom) = ($2, $1);
    print "\"$str\", (nsIAtom**)&${classname}::$atom\n";
  }
}
