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
# build script (debug)
#

use Cwd;
use MozillaBuildCore;

#-------------------------------------------------------------
# Where have the build options gone?
# 
# The various build flags have been centralized into one place. 
# The master list of options is in MozBuildFlags.pm. However, 
# you should never need to edit that file, or this one.
# 
# To customize what gets built, or where to start the build, 
# edit the 'Mozilla debug build prefs' file in
# System Folder:Preferences:Mozilla build prefs.
# Documentation is provided in that file.
#-------------------------------------------------------------


#-------------------------------------------------------------
# hashes to hold build options
#-------------------------------------------------------------
my(%pull);
my(%build);
my(%options);
my(%filepaths);
my(%optiondefines);

#-------------------------------------------------------------
# configuration variables that globally affect what is built
#-------------------------------------------------------------
$DEBUG                  = 1;
$CARBON                 = 0;    # turn on to build with TARGET_CARBON
$PROFILE                = 0;
$RUNTIME                = 0;    # turn on to just build runtime support and NSPR projects
$GC_LEAK_DETECTOR       = 0;    # turn on to use GC leak detection
$MOZILLA_OFFICIAL       = 0;    # generate build number
$LOG_TO_FILE            = 0;    # write perl output to a file

#-------------------------------------------------------------
# configuration variables that affect the manner of building, 
# but possibly affecting the outcome.
#-------------------------------------------------------------
$BIN_DIRECTORY          = ":mozilla:dist:viewer_debug:";

$ALIAS_SYM_FILES        = $DEBUG;
$CLOBBER_LIBS           = 1;    # turn on to clobber existing libs and .xSYM files before
                                # building each project                         
# The following two options will delete all dist files (if you have $build{dist} turned on),
# but leave the directory structure intact.
$CLOBBER_DIST_ALL       = 1;    # turn on to clobber all aliases/files inside dist (headers/xsym/libs)
$CLOBBER_DIST_LIBS      = 0;    # turn on to clobber only aliases/files for libraries/sym files in dist
$CLOBBER_IDL_PROJECTS   = 0;    # turn on to clobber all IDL projects.

$UNIVERSAL_INTERFACES_VERSION = 0x0320;

#-------------------------------------------------------------
# configuration variables that are preferences for the build,
# style and do not affect what is built.
#-------------------------------------------------------------
$CodeWarriorLib::CLOSE_PROJECTS_FIRST
                        = 1;
                                # 1 = close then make (for development),
                                # 0 = make then close (for tinderbox).
$USE_TIMESTAMPED_LOGS   = 0;
#-------------------------------------------------------------
# END OF CONFIG SWITCHES
#-------------------------------------------------------------



my($cur_dir) = cwd();
$cur_dir =~ s/:mozilla:build:mac:build_scripts$//;
chdir($cur_dir);
$MOZ_SRC = cwd();

my($do_checkout)    = 0;
my($do_build)       = 1;

RunBuild($do_checkout, $do_build, "MozillaBuildFlags.txt", "Mozilla debug build prefs");
