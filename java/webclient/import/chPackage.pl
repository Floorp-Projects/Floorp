# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Sun Microsystems,
# Inc. Portions created by Sun are
# Copyright (C) 1999 Sun Microsystems, Inc. All
# Rights Reserved.
#
# Contributor(s): 
# Ashu Kulkarni <ashuk@eng.sun.com>
#


# this script must be run in the directory in which it resides.

#
# Verification, usage checking
#

$ARGC = $#ARGV + 1;
$NUM_ARGC = 3;

if ($NUM_ARGC != $ARGC) {
  print "usage: chPackage.pl <OS> <file name> <package name>\n";
  exit -1;
}

if ($ARGV[0] ne "unix" && $ARGV[0] ne "win") {
  print "usage: chPackage.pl <OS> <file name> <package name>\n";
  print "       <OS> can be either \"unix\" or \"win\" \n";
}

open(FILE_R, "<$ARGV[1]");
open(FILE_W, ">tmp.tmp");

while ($line = <FILE_R>) {
  if ($line =~ "package") {
    $line_w = "package $ARGV[2];";
    print FILE_W $line_w;
    print FILE_W "\n\nimport org.mozilla.xpcom.*;\n";
  }
  else {
    print FILE_W $line;
  }
}

close(FILE_R);
close(FILE_W);

if ($ARGV[0] eq "unix") {
  $cmd = "mv tmp.tmp $ARGV[1]; rm -f tmp.tmp";
  exec $cmd;
}
else {
  $cmd = "copy tmp.tmp $ARGV[1]";
  exec $cmd;
  $cmd = "del tmp.tmp";
  exec $cmd;
}


