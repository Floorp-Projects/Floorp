#!perl

#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#

#
# build script (optimized)
#
use Mac::Processes;
use NGLayoutBuildList;
use Cwd;
use Moz;

#-----------------------------------------------
# configuration variables that globally affect what is built
#-----------------------------------------------
$DEBUG					= 0;
$CARBON					= 0;	# turn on to build with TARGET_CARBON
$PROFILE				= 0;
$GC_LEAK_DETECTOR		= 0;	# turn on to use GC leak detection
$INCLUDE_CLASSIC_SKIN 	= 1; 

$pull{all} 				= 0;
$pull{moz}				= 0;
$pull{runtime} 			= 0;

$build{all}             = 1;	# turn off to do individual builds, or to do "most"
$build{most}            = 0;	# turn off to do individual builds
$build{dist}            = 0;
$build{dist_runtime}    = 0;	# implied by $build{dist}
$build{xpidl}           = 0;
$build{idl}             = 0;
$build{stubs}           = 0;
$build{runtime}         = 0;
$build{common}          = 0;
$build{imglib}          = 0;
$build{necko}           = 0;
$build{security}        = 0;
$build{browserutils}    = 0;
$build{intl}            = 0;
$build{nglayout}        = 0;
$build{editor}          = 0;
$build{viewer}          = 0;
$build{xpapp}           = 0;
$build{extensions}      = 1;
$build{plugins}         = 0;
$build{mailnews}        = 0;
$build{apprunner}       = 0;
$build{resources}       = 1;

$build{xptlink}		    = 0;

$options{transformiix}	= 0;
$options{mathml}		= 0;
$options{svg}			= 0;
$options{mng}			= 1;
$options{ldap}			= 0;
$options{xmlextras}		= 0;

# -------------------------- Chrome and jar-related options -----------------------------------------

$options{jar_manifests} = 1;        # use jar.mn files for resources, not MANIFESTs. This must be ON,
                                    # unless you want to use the obsolete (and probably bitrotted)
                                    # MANIFEST files to install resources.


# These two options determine whether you get jar files, or loose files in chrome.
# Either one or both option must be turned on.
                                                                        
$options{chrome_jars}   = 1;        # build jar files
$options{chrome_files}  = 0;        # install regular files in chrome

$options{use_jars}      = 1;        # This option determines whether chrome comes out of jars
                                    # or regular files at runtime (by affecting installed-chrome.txt).
                                    # If 1, $options{chrome_jars} must be 1.

# ------------------------ End chrome and jar-related options ----------------------------------------

# Don't change these (where should they go?)
$optiondefines{mathml}{MOZ_MATHML}		= 1;
$optiondefines{svg}{MOZ_SVG}			= 1;

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
						= 0;
								# 1 = close then make (for development),
								# 0 = make then close (for tinderbox).
$USE_TIMESTAMPED_LOGS 	= 0;
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
#   $build{dist}            = 0;
#   $build{dist_runtime}    = 0;	# Implied by $build{dist}
#   $build{xpidl}           = 0;
#   $build{idl}             = 0;
#   $build{stubs}           = 0;
#   $build{runtime}         = 1;
#   $build{common}          = 1;
#   $build{imglib}          = 1;
#   $build{necko}           = 1;
#   $build{security}        = 1;
#   $build{browserutils}    = 1;
#   $build{intl}            = 1;
#   $build{nglayout}        = 1;
#   $build{editor}          = 1;
#   $build{viewer}          = 1;
#   $build{xpapp}           = 1;
#   $build{extensions}      = 1;
#   $build{plugins}         = 1;
#   $build{mailnews}        = 1;
#   $build{apprunner}       = 1;
#   $build{resources}       = 0;
}

# do the work
# you should not have to edit anything below

chdir("::::");
$MOZ_SRC = cwd();

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
	OpenErrorLog("NGLayoutBuildLog");		# Release build requires that name
	#OpenErrorLog("Mozilla.BuildLog");		# Tinderbox requires that name
}

Moz::StopForErrors();
#Moz::DontStopForErrors();

Checkout();

ConfigureBuildSystem();

my(@gen_files) = (
    ":mozilla:xpfe:appshell:public:nsBuildID.h",
    ":mozilla:xpfe:browser:resources:locale:en-US:navigator.dtd"
);
SetBuildNumber(":mozilla:config:build_number", ":mozilla:config:aboutime.pl", \@gen_files);

chdir($MOZ_SRC);
BuildDist();

chdir($MOZ_SRC);
BuildProjects();

print "Build complete\n";
