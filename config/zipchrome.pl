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
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation. Portions created by Netscape are
# Copyright (C) 1998-1999 Netscape Communications Corporation. All
# Rights Reserved.
# 
# Contributor(s): 
# Sean Su <ssu@netscape.com>
# 

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

