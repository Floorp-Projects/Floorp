#!perl
# 
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Sean Su <ssu@netscape.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# Make sure there are at least four arguments
if(($#ARGV < 2) ||
  ((!($ARGV[0] =~ /^win32$/i)) &&
   (!($ARGV[0] =~ /^mac$/i)) &&
   (!($ARGV[0] =~ /^unix$/i))) ||
  ((!($ARGV[1] =~ /^update$/i)) &&
   (!($ARGV[1] =~ /^noupdate$/i))))
{
  PrintUsage();
  exit(1);
}

require "$ENV{MOZ_SRC}/mozilla/config/zipcfunc.pl";

if($#ARGV == 2)
{
  if(&ZipChrome($ARGV[0], $ARGV[1], $ARGV[2], $ARGV[2]) != 0)
  {
    die "\n Error: ZipChrome($ARGV[0], $ARGV[1], $ARGV[2], $ARGV[2])\n";
  }
}
else
{
  if(&ZipChrome($ARGV[0], $ARGV[1], $ARGV[2], $ARGV[3]) != 0)
  {
    die "\n Error: ZipChrome($ARGV[0], $ARGV[1], $ARGV[2], $ARGV[3])\n";
  }
}
exit(0);

sub PrintUsage()
{
  print "usage: $0 <os> <update> <source path> [target path]

       os                : win32, mac, unix

       update            : update    - enables time/date compare file update of chrome archives
                           noupdate  - disables time/date compare file update of chrome archives.
                                       it will always update chrome files regardless of time/date stamp.

       source path       : path to where the chrome dir is at

       target path       : (optional) path to where the chrome jar files should be copied to

              ie: $0 update $ENV{MOZ_SRC}\\mozilla\\dist\\win32_d.obj\\tmpchrome $ENV{MOZ_SRC}\\mozilla\\dist\\win32_d.obj\\bin\\chrome
       \n";
}

