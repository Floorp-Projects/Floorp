#!perl
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Jamie Zawinski <jwz@mozilla.org>
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

#
# Input: output_class_dir argfile javafilelist
#

$classdir = $ARGV[0];
$argfile = $ARGV[1];
$filelist = $ARGV[2];
shift;

$sjcmd = $ENV{'MOZ_TOOLS'} . '\bin\sj.exe';

open(FL, "<$filelist" ) || die "can't open $filelist for reading";

while( <FL> ){
    # print;
    @java_files = (@java_files, split(/[ \t\n]/));
}
close (FL);

open(FL, "<$argfile" ) || die "can't open $argfile for reading";

while( <FL> ){
    chop;
    $args .= $_;
}
close (FL);


print "Java Files: @java_files\n";


# compile as many java files as we can in one invocation
# given the limitations of the command line
foreach $filename (@java_files) {
    $classfilename = $classdir;
    $classfilename .= $filename;
    $classfilename =~ s/.java$/.class/;
    ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,
     $ctime,$blksize,$blocks) = stat($filename);
    ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$classmtime,
     $ctime,$blksize,$blocks) = stat($classfilename);
#    print $filename, " ", $mtime, ", ", $classfilename, " ", $classmtime, "\n";
    if ($mtime > $classmtime) {
# are we too big?	  
		$len += length($filename);
		if( $len >= 512 ) {
			print "$sjcmd $args $outofdate\n";
			$status = system "$sjcmd $args $outofdate";
			if( $status != 0 ) {
				exit( $status / 256 );
			}
			$outofdate = "";
			$len = length($filename);
		}
		$outofdate .= $filename;
		$outofdate .= " ";
    }
}

if( length($outofdate) > 0 ) {
	print "$sjcmd $args $outofdate\n";
	$status = system "$sjcmd $args $outofdate";
	if( $status != 0 ) {
		exit( $status / 256 );
	}
} else {
	print "Files are up to date.\n";
}


print "\n";
