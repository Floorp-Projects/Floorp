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
# NSPR build script (debug)
#
use Mac::Processes;
use NGLayoutBuildList;
use Cwd;
use Moz;

# configuration variables
$DEBUG = 1;
$ALIAS_SYM_FILES = $DEBUG;
$CLOBBER_LIBS = 1;			# turn on to clobber existing libs and .xSYM files before
							# building each project
$MOZ_FULLCIRCLE = 0;

# The following two options will delete all files, but leave the directory structure intact.
$CLOBBER_DIST_ALL = 0;      # turn on to clobber all files inside dist (headers, xsym and libs)
$CLOBBER_DIST_LIBS = 0;     # turn on to clobber the aliases to libraries and sym files in dist

$pull{all} = 0;
$pull{runtime} = 1;


$build{all} = 0;			# turn off to do individual builds
$build{dist} = 0;
$build{dist_runtime} = 1;
$build{runtime} = 1;
$build{stubs} = 1;


# do the work
# you should not have to edit anything bellow

chdir("::::");
$MOZ_SRC = cwd();


OpenErrorLog("Runtime.BuildLog");
#OpenErrorLog("Mozilla.BuildLog");		# Tinderbox requires that name

Moz::StopForErrors();
#Moz::DontStopForErrors();

Checkout();

chdir($MOZ_SRC);
BuildDist();

chdir($MOZ_SRC);
BuildProjects();

print "Runtime Build complete\n";
