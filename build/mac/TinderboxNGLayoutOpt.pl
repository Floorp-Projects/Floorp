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
use NGLayoutBuildList;
use Cwd;
use Moz;


# configuration variables

$DEBUG = 0;
$ALIAS_SYM_FILES = 0;
$CLOBBER_LIBS = 1;
$MOZ_FULLCIRCLE = 0;

$pull{all} = 1;
$pull{lizard} = 0;
$pull{xpcom} = 0;
$pull{imglib} = 0;
$pull{netlib} = 0;
$pull{nglayout} = 0;
$pull{mac} = 0;

$build{all} = 1;
$build{dist} = 0;
$build{stubs} = 0;
$build{common} = 0;
$build{nglayout} = 0;
$build{resources} = 0;
$build{editor} = 0;
$build{viewer} = 0;
$build{xpapp} = 0;
$build{mailnews} = 0;


# script


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

OpenErrorLog("::::Mozilla.BuildLog");		# Tinderbox requires that name

chdir("::::");

Checkout();

BuildDist();

BuildProjects();

print "Build NGLayout complete\n";
