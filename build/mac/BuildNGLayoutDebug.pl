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
# build script (debug)
#
use Mac::Processes;
use NGLayoutBuildList;
use Cwd;
use Moz;

#-----------------------------------------------
# configuration variables that globally affect what is built
#-----------------------------------------------
$BUILD_ROOT				= ":";
								# $BUILD_ROOT = ":", the default, with $build{all}
								# builds the whole tree. Leaving $BUILD_ROOT undefined
								# will also have this effect.
								#
								# $BUILD_ROOT = ":mozilla:mailnews:" with $build{all}
								# will "make everything in the mailnews directory".
								#
								# This is equivalent to the unix commands:
								#   cd mozilla/mailnews
								#   gmake
								# or the windows commands:
								#   cd mozilla\mailnews
								#   nmake -f makefile.win
								#
								# In conjunction with build{all}, this will carry out
								# all build phases (export, build, install...), but only
								# using files inside $BUILD_ROOT. Replace $build{all}
								# with specific component flags to build stuff from
								# those components that happens to live inside
								# $BUILD_ROOT.
								#
								# ":", and ":mozilla:" both have the same effect, because
								# this works by checking that path strings begin with
								# $BUILD_ROOT.
								#
								# $BUILD_ROOT affects only the build phase, not the pull
								# phase. There is still no scriptable way to update-and-build
								# a specific directory. In particular, $pull{all} will
								# still pull the whole tree, irrespective of the
								# setting of $BUILD_ROOT. To update and build a particular
								# directory, use MacCVS Pro to update the directory,
								# then turn off all $pull variables, and set $BUILD_ROOT.
$DEBUG					= 1;
$CARBON					= 0;	# Turn on to build with TARGET_CARBON
$NECKO					= 1;
$MOZ_FULLCIRCLE			= 0;
$PROFILE				= 0;

$pull{all} 				= 0;
$pull{lizard} 			= 0;
$pull{xpcom} 			= 0;
$pull{imglib} 			= 0;
$pull{netlib} 			= 0;
$pull{nglayout} 		= 0;
$pull{mac} 				= 0;

$build{all} 			= 1;	# Turn off to do individual builds, or to do "most"
$build{most} 			= 1;	# Turn off to do individual builds
$build{dist} 			= 0;
$build{dist_runtime}	= 0;	# Implied by $build{dist}
$build{resources} 		= 0;
$build{idl}             = 0;
$build{xpidl}			= 0;
$build{stubs} 			= 0;
$build{runtime}			= 0;
$build{common} 			= 0;
$build{intl} 			= 0;
$build{nglayout} 		= 0;
$build{editor} 			= 0;
$build{viewer} 			= 0;
$build{xpapp} 			= 0;
$build{mailnews} 		= 0;
$build{apprunner} 		= 0;

#-----------------------------------------------
# configuration variables that affect the manner
# of building, but possibly affecting
# the outcome.
#-----------------------------------------------
$ALIAS_SYM_FILES		= $DEBUG;
$CLOBBER_LIBS			= 1;	# turn on to clobber existing libs and .xSYM files before
								# building each project							
# The following two options will delete all dist files (if you have $build{dist} turned on),
# but leave the directory structure intact.
$CLOBBER_DIST_ALL 		= 1;	# turn on to clobber all aliases/files inside dist (headers/xsym/libs)
$CLOBBER_DIST_LIBS 		= 0;	# turn on to clobber only aliases/files for libraries/sym files in dist
$CLOBBER_IDL_PROJECTS	= 0;	# turn on to clobber all IDL projects.

#-----------------------------------------------
# configuration variables that are preferences for the build style,
# and do not affect what is built.
#-----------------------------------------------
$CodeWarriorLib::CLOSE_PROJECTS_FIRST
						= 1;
								# 1 = close then make (for development),
								# 0 = make then close (for tinderbox).
$USE_TIMESTAMPED_LOGS 	= 1;
#-----------------------------------------------
# END OF CONFIG SWITCHES
#-----------------------------------------------

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
if ($build{most})
{
### Just uncomment/comment to get the ones you want (if "most" is selected).
#	$build{dist}		= 1;
#	$build{dist_runtime}= 1;
#	$build{resources}	= 1;
	$build{idl}         = 1;
	$build{stubs}		= 1;
	$build{runtime}		= 1;
	$build{common}		= 1; # Requires intl
	$build{intl}		= 1; 
	$build{nglayout}	= 1;
	$build{editor}		= 1;
	$build{viewer}		= 1;
	$build{xpapp}		= 1;
	$build{mailnews}	= 1;
	$build{apprunner} 	= 1;
}

# do the work
# you should not have to edit anything below

chdir("::::");
$MOZ_SRC = cwd();

if ($MOZ_FULLCIRCLE)
{
	#// Get the Build Number for the Master.ini(Full Circle) n'stuff
	$buildnum = Moz::SetBuildNumber();
}

if ($USE_TIMESTAMPED_LOGS)
{
	#Use time-stamped names so that you don't clobber your previous log file!
	my $now = localtime();
	while ($now =~ s@:@.@) {} # replace all colons by periods
	my $logdir = ":Build Logs:";
	if (!stat($logdir))
	{
	        print "Creating directory $logdir\n";
	        mkdir $logdir, 0777 || die "Couldn't create directory $logdir";
	}
	OpenErrorLog("$logdir$now");
}
else
{
	OpenErrorLog("NGLayoutBuildLog");		# Release build
	#OpenErrorLog("Mozilla.BuildLog");		# Tinderbox requires that name
}

Moz::StopForErrors();
#Moz::DontStopForErrors();

if ($pull{all}) { 
   Checkout();
}


BuildDist();

chdir($MOZ_SRC);
BuildProjects();

print "Build complete\n";
