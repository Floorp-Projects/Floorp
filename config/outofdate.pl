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
#Input: [-d dir] foo1.java foo2.java
#Compares with: foo1.class foo2.class (if -d specified, checks in 'dir', 
#  otherwise assumes .class files in same directory as .java files)
#Returns: list of input arguments which are newer than corresponding class
#files (non-existant class files are considered to be real old :-)
#

$found = 1;

# GLOBALS
$SEP = 0; # the paltform independent path separator
$CFG = 0; # the value of the -cfg flag

# determine the path separator
$_ = $ENV{"PATH"};
if (m|/|) {
	$SEP = "/";
}
else {
	$SEP = "\\";
}

if ($ARGV[0] eq '-d') {
    $classdir = $ARGV[1];
    $classdir .= $SEP;
    shift;
    shift;
} else {
    $classdir = "." . $SEP;
}

# if -cfg is specified, print out the contents of the cfg file to stdout
if ($ARGV[0] eq '-cfg') {
    $CFG = $ARGV[1];
    shift;
    shift;
} 

$_ = $ARGV[0];
if (m/\*.java/) {
	# Account for the fact that the shell didn't expand *.java by doing it
	# manually.
	&manuallyExpandArgument("java");
}

$printFile = 0;

foreach $filename (@ARGV) {
    $classfilename = $classdir;
    $classfilename .= $filename;
    $classfilename =~ s/.java$/.class/;
# workaround to only build sun/io/* classes when necessary
# change the pathname of target file to be consistent
# with sun/io subdirectories
#
# sun/io was always getting rebuilt because the java files
# were split into subdirectories, but the package names
# remained the same.  This was confusing outofdate.pl
#
    $classfilename =~ s/sun\/io\/extended.\//sun\/io\//;
    $classfilename =~ s/\.\.\/\.\.\/sun-java\/classsrc\///;
    ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,
     $ctime,$blksize,$blocks) = stat($filename);
    ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$classmtime,
     $ctime,$blksize,$blocks) = stat($classfilename);
#    print $filename, " ", $mtime, ", ", $classfilename, " ", $classmtime, "\n";
    if ($mtime > $classmtime) {

		# Only print the file header if we actually have some files to
		# compile.
		if (!$printFile) {
			$printFile = 1;
			&printFile($CFG);
		}
        print $filename, " ";
        $found = 0;
    }
}

print "\n";

# push onto $ARG array all filenames with extension $ext

# @param ext the extension of the file

sub manuallyExpandArgument {
	local($ext) = @_;
	$ext = "\." . $ext;			# put it in regexp

	$result = opendir(DIR, ".");

	@allfiles = grep(/$ext/, readdir(DIR));
	$i = 0;
	foreach $file (@allfiles) {
		#skip emacs save files
		$_ = $file;
		if (!/~/) {
			$ARGV[$i++] = $file;
		}
	}
}

sub printFile {
	local($file) = @_;

	$result = open(CFG, $file);
	while (<CFG>) {
		chop;
		print $_;
	}
}
