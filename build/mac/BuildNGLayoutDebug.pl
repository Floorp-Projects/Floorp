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
$DEBUG = 1;
$pull{all} = 0;
$pull{lizard} = 0;
$pull{xpcom} = 0;
$pull{imglib} = 0;
$pull{netlib} = 0;
$pull{nglayout} = 0;
$pull{mac} = 0;

$build{all} = 0;
$build{dist} = 0;
$build{projects}= 0;

#
# UI
#

@choices = ("pull_and_build_all", 
						"pull_all", 
						"build_all",  
						"pull_nglayout", 
						"build_dist", 
						"build_projects");
#damn, this does not work on 
if (0)
{
	@pick = MacPerl::Pick("What would you like to do?", @choices);
	$pull{all} = 0;
	$build{all} = 1;
	foreach $i (@pick)
	{
		if ($i eq "pull_and_build_all")
		{
			$pull{all} = 1;
			$build{all} = 1;
		}
		elsif ($i eq "pull_all")
		{
			$pull{all} = 1;
		}
		elsif ($i eq "build_all")
		{
			$build{all} = 1;
		}
		elsif ($i eq "build_dist")
		{
			$build{dist} = 1;
		}
		elsif ($i eq "build_projects")
		{
			$build{projects} = 1;
		}
	}
}
else
{
	$pull{all} = 1;
	$build{all} = 1;
#	$build{projects} = 1;
#	$build{dist} = 1;
#	$pull{nglayout} = 1;
}

if ($pull{all})
{
	foreach $k (keys(%pull))
	{
		$pull{$k} = 1;
	}
}
if ($build{all})
{
	foreach $k (keys(%build))
	{
		$build{$k} = 1;
	}
}

# do the work
# you should not have to edit anything bellow

chdir("::::");
$MOZ_SRC = cwd();
Moz::StopForErrors();
#Moz::DontStopForErrors();
OpenErrorLog("::NGLayoutBuildLog");

Checkout();
chdir($MOZ_SRC);
BuildDist();
chdir($MOZ_SRC);
BuildProjects();
print "Build layout complete\n";
