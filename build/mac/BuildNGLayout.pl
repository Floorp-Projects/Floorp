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
# nglayout build script (debug)
#
use Mac::Processes;
use NGLayoutBuildList;
use Cwd;
use Moz;

# configuration variables
$DEBUG = 0;
$ALIAS_SYM_FILES = $DEBUG;
$CLOBBER_LIBS = 1;
$MOZ_FULLCIRCLE = 0;

$pull{all} = 0;
$pull{lizard} = 0;
$pull{xpcom} = 0;
$pull{imglib} = 0;
$pull{netlib} = 0;
$pull{nglayout} = 0;
$pull{mac} = 0;

$build{all} = 1;				# turn off to do individual builds
$build{dist} = 0;
$build{stubs} = 0;
$build{common} = 0;
$build{nglayout} = 0;
$build{resources} = 0;
$build{editor} = 0;
$build{viewer} = 0;
$build{xpapp} = 0;
$build{mailnews} = 0;

if ($pull{all})
{
	foreach $k (keys(%pull))
	{
		$pull{$k} = 1;
	}
}
if ($build{all})
{
	$temp = $build{mailnews};

	foreach $k (keys(%build))
	{
		$build{$k} = 1;
	}
	
	$build{mailnews} = $temp;	# don't turn on mailnews until we are sure that everything is ok on Tinderbox
}

# do the work
# you should not have to edit anything bellow

chdir("::::");
$MOZ_SRC = cwd();

OpenErrorLog("NGLayoutBuildLog");
#OpenErrorLog("Mozilla.BuildLog");		# Tinderbox requires that name

Moz::StopForErrors();
#Moz::DontStopForErrors();

if ($pull{all}) { 
   Checkout();
}

SetBuildNumber();

chdir($MOZ_SRC);
BuildDist();

chdir($MOZ_SRC);
BuildProjects();

print "Build layout complete\n";

