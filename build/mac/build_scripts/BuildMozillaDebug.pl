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
#   Simon Fraser <sfraser@netscape.com>
#

require 5.004;

use strict;

use Cwd;
use Moz::BuildUtils;
use Moz::BuildCore;

#-------------------------------------------------------------
# Where have the build options gone?
# 
# The various build flags have been centralized into one place. 
# The master list of options is in MozBuildFlags.txt. However, 
# you should never need to edit that file, or this one.
# 
# To customize what gets built, or where to start the build, 
# edit the $prefs_file_name file in
# System Folder:Preferences:Mozilla build prefs:
# Documentation is provided in that file.
#-------------------------------------------------------------

my($prefs_file_name) = "Mozilla debug build prefs";
my($config_header_file_name) = ":mozilla:config:mac:DefinesOptionsDebug.h";

#-------------------------------------------------------------
# hashes to hold build options
#-------------------------------------------------------------
my(%build);
my(%options);
my(%filepaths);
my(%optiondefines);

# Hash of input files for this build. Eventually, there will be
# input files for manifests, and projects too.
my(%inputfiles) = (
  "buildflags",     "MozillaBuildFlags.txt",
  "checkoutdata",   "MozillaCheckoutList.txt",
  "buildprogress",  "¥ Mozilla debug progress",
  "buildmodule",    "MozillaBuildList.pm",
  "checkouttime",   "Mozilla last checkout"
);
#-------------------------------------------------------------
# end build hashes
#-------------------------------------------------------------

# set the build root directory, which is the the dir above mozilla
SetupBuildRootDir(":mozilla:build:mac:build_scripts");

# Set up all the flags on $main::, like DEBUG, CARBON etc.
# Override the defaults using the preferences files.
SetupDefaultBuildOptions(1, ":mozilla:dist:viewer_debug:", $config_header_file_name);

my($do_pull)        = 0;    # overridden by flags and prefs
my($do_build)       = 1;

RunBuild($do_pull, $do_build, \%inputfiles, $prefs_file_name);
