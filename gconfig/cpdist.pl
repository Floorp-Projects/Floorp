#! /usr/local/bin/perl
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
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

require('gconfig.pl');

#######-- read in variables on command line into %var

&parse_argv;
 
### do the copy

print STDERR "RELEASE TREE / MODULE =  $var{RELEASE_TREE} $var{MODULE}\n";



# 1
if ($var{RELEASE} eq "") { exit; } # Can't do release here, so exit.

# 2
#if (! ($var{RELEASE} =~ /\//)) {   # if no specific version is specified in RELEASE variable
#    $component = $var{RELEASE};
#}
#else {  # if a subcomponent/version is given in the RELEASE variable
#        $var{RELEASE} =~ m|^([^/]*)/|;  
#	$component = $1;           # everything before the first slash;
#    }

# 3
$path = $var{RELEASE};


# 4
# find out what directory we would create for 'today'

$year = (localtime)[5] + 1900;
$month = (localtime)[4] + 1;
$day = (localtime)[3];
$today = sprintf( "%d%02d%02d", $year, $month, $day );

# 5
# if version is null, then set the version to today.
if ($var{"RELEASE_VERSION"} eq "") {
    $var{"RELEASE_VERSION"} = $today;
}

#6
$version = $var{"RELEASE_VERSION"};  # set RELEASE_VERSION to passed in variable

#7
# if version is today, then we will want to make a 'current' link.

if ($version eq $today) {
    $create_current = 1;
}

#8
# version can be a) passed in value from command line, b) value in manifest.mn
# or c) computed value such as '19970909'


$dir = "$var{'RELEASE_TREE'}/$path";

#9
if (! (-e "$dir/$version" && -d "$dir/$version")) {
    print "making dir $dir \n";
    &rec_mkdir("$dir/$version");
}



print "version = $version\n";
print "path = $path\n";
print "var{release_tree} = $var{'RELEASE_TREE'}\n";
print "dir = $dir   = RELEASE_TREE/path\n";


#10
if ($create_current == 1) {

# unlinking and linking always occurs, even if the link is correct
    print "unlinking $dir/current\n";
    unlink("$dir/current");
    
    print "putting version number $today into 'current' file..";

    open(FILE,">$dir/current") || die " couldn't open current\n";
    print FILE "$today\n";
    close(FILE);
    print " ..done\n"
    
}

&rec_mkdir("$dir/$version/$var{'RELEASE_MD_DIR'}");
&rec_mkdir("$dir/$version/$var{'RELEASE_XP_DIR'}");




foreach $jarfile (split(/ /,$var{FILES}) ) {
    print STDERR "---------------------------------------------\n";
    
    $jarinfo = $var{$jarfile};
 
    ($jardir,$jaropts) = split(/\|/,$jarinfo);
 
    if ($jaropts =~ /f/) {
      print STDERR "Copying files $jardir....\n";
    }
    else {
      print STDERR "Copying jar file $jarfile....\n";
    }
    
    print "jaropts = $jaropts\n";
    
    if ($jaropts =~ /m/) {
      $destdir = $var{"RELEASE_MD_DIR"};
      print "found m, using MD dir $destdir\n";
    }
    elsif ($jaropts =~ /x/) {
      $destdir = $var{"RELEASE_XP_DIR"};
      print "found x, using XP dir $destdir\n";
    }
    else {
      die "Error: must specify m or x in jar options in $jarinfo line\n";
    }
    
    
    $distdir = "$dir/$version/$destdir";
    


    if ($jaropts =~ /f/) {

      print "splitting: \"$jardir\"\n";
      for $srcfile (split(/ /,$jardir)) {

#if srcfile has a slash
	if ($srcfile =~ m|/|) {
#pull out everything before the last slash into $1
	  $srcfile =~ m|(.*)/|;
	  $distsubdir = "/$1";
	  print "making dir $distdir$distsubdir\n";
	  &rec_mkdir("$distdir$distsubdir");
	}
	print "copy: from $srcfile\n";
	print "      to   $distdir$distsubdir\n";
	$srcprefix = "";
	if ($jaropts =~/m/) {
	  $srcprefix = "$var{'PLATFORM'}/";
	}
	system("cp $srcprefix$srcfile $distdir$distsubdir");
      }
    }
    else {
      $srcfile = "$var{SOURCE_RELEASE_PREFIX}/$jardir/$jarfile";
      
      print "copy: from $srcfile\n";
      print "      to   $distdir\n";
      
      system("cp $srcfile $distdir");
      
    }
    
  }

