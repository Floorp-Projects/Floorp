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
# nglayout pull script 
#
use Mac::Processes;
use MozillaBuildList;
use Cwd;
use Moz;

#-----------------------------------------------
# hashes to hold build options
#-----------------------------------------------
my(%pull);
my(%build);
my(%options);
my(%optiondefines);


my($cur_dir) = cwd();
$cur_dir =~ s/:mozilla:build:mac:build_scripts$//;
chdir($cur_dir);
$MOZ_SRC = cwd();

my($do_checkout)    = 1;
my($do_build)       = 0;

RunBuild($do_checkout, $do_build);
