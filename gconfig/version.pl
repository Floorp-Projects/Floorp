#!/usr/sbin/perl
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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

# Compose lowercase alphabet
@alphabet = ( "a", "b", "c", "d", "e", "f", "g", "h",
              "i", "j", "k", "l", "m", "n", "o", "p",
              "q", "r", "s", "t", "u", "v", "w", "x",
              "y", "z" );

# Compute year
$year = (localtime)[5] + 1900;

# Compute month
$month = (localtime)[4] + 1;

# Compute day
$day = (localtime)[3];

# Compute base build number
$version = sprintf( "%d%02d%02d", $year, $month, $day );
$directory = sprintf( "%s\/%s\/%d%02d%02d", $ARGV[0], $ARGV[1], $year, $month, $day );

# Print out the name of the first version directory which does not exist
#if( ! -e  $directory )
#{
    print $version;
#}
#else
#{
#    # Loop through combinations
#    foreach $ch1 (@alphabet)
#    {
#	foreach $ch2 (@alphabet)
#	{
#	    $version = sprintf( "%d%02d%02d%s%s", $year, $month, $day, $ch1, $ch2 );
#	    $directory = sprintf( "%s\/%s\/%d%02d%02d%s%s", $ARGV[0], $ARGV[1], $year, $month, $day, $ch1, $ch2 );
#	    if( ! -e  $directory )
#	    {
#		print STDOUT $version;
#		exit;
#	    }
#	}
#    }
#}

