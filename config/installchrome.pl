#!perl -w
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

# Make sure there are at least two arguments
if($#ARGV < 1)
{
  die "usage: $0 <os> <type> <target path>

       os         : mac, unix, win32
       type       : jar or resource
       target path: path to where the chrome dir is at

              ie: $0 win32 resource $ENV{MOZ_SRC}\\mozilla\\dist\\win32_d.obj\\bin\\chrome
       \n";
}

require "$ENV{MOZ_SRC}/mozilla/config/installcfunc.pl";

if(&InstallChrome($ARGV[0], $ARGV[1], $ARGV[2]) != 0)
{
  die "\n Error: InstallChrome($ARGV[0], $ARGV[1], $ARGV[2]): $!\n";
}

