#!perl
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
#Input: [-d dir] foo1.java foo2.java
#Compares with: foo1.class foo2.class (if -d specified, checks in 'dir', 
#  otherwise assumes .class files in same directory as .java files)
#Returns: list of input arguments which are newer than corresponding class
#files (non-existant class files are considered to be real old :-)
#

$found = 1;

if ($ARGV[0] eq '-d') {
    $classdir = $ARGV[1];
    $classdir .= "/";
    shift;
    shift;
} else {
    $classdir = "./";
}

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
    ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$mtime,
     $ctime,$blksize,$blocks) = stat($filename);
    ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,$atime,$classmtime,
     $ctime,$blksize,$blocks) = stat($classfilename);
#    print $filename, " ", $mtime, ", ", $classfilename, " ", $classmtime, "\n";
    if ($mtime > $classmtime) {
        print $filename, " ";
        $found = 0;
    }
}

print "\n";
