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

	use Moz;
	use BuildList;

$DEBUG = 1;

	# One of them should be 1. This will come from a config file at some stage.
	# In the meanwhile, it should match mozilla/config/mac/MacConfig.h
$MOZ_LITE   = 0;
$MOZ_MEDIUM = 1;
$MOZ_DARK   = 0;

Moz::OpenErrorLog(":::Mozilla.BuildLog");
Moz::StopForErrors();

chdir("::::");

SetBuildNumber();

SetTimeBomb(0, 30);

# Make and popuplate the dist directory
DistMozilla();

Delay(10);

# Set the build number in about-all.html.  Commented out for now
# until ckid/mcvs resource problem is resolved.

# SetAgentString();

# Now build the projects
BuildMozilla();
