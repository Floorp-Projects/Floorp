#!/usr/bin/perl -w

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
# The Original Code is the Chrome/CVS Location Matcher.
#
# The Initial Developer of the Original Code is
# Gervase Markham.
# Portions created by the Initial Developer are Copyright (C) 2001
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

# Version 0.5
#
# This file creates a large list of the mappings between chrome path and CVS
# paths which are recorded in the jar.mn files throughout the tree. This list
# is shipped with builds to make it easier for people to create chrome patches
# using them.

use Fcntl qw(:DEFAULT :flock);
use File::Basename;
use File::Spec;
use Cwd;
use mozLock;

# This file takes two parameters - the jar file to process, and the chrome
# directory we are compiling into.
my ($chrome, $jar, $flock) = @ARGV;

my $nofilelocks = $flock ? ($flock eq "-l") : 0;

# OS setup - which chrome does your OS use?
#
# The $^O variable contains some representation of what OS you are on.
# Find its value on your platform by running the following command:
# perl -e 'print $^O . "\n";'
# and set the relevant booleans accordingly depending on what
# chrome your platform uses. There are currently three sorts of chrome:
# win, mac, and unix.
 
my $win32 = ($^O =~ /((MS)?win32)|os2/i) ? 1 : 0;
my $unix  = ($^O =~ /linux/i)            ? 1 : 0;
my $macos = ($^O =~ /MacOS|darwin/i)     ? 1 : 0;

# Testing only - generate chromelist.txt for other platforms
# $macos = 1; 
# $unix = 0;

my $chromelist = File::Spec->catfile("$chrome", "chromelist.txt");
my $lockfile = $chromelist . ".lck";

mozLock($lockfile) unless $nofilelocks;

open(BIGLIST, ">>" . $chromelist) or die "Can't open chromelist.txt";
open(JARFILE, "<$jar");

# Find the absolute directory the jar.mn file is in
my $stub = File::Spec->catdir(getcwd(), dirname($jar));

# Convert back to Unix-style directory names for the CVS version
if ($macos) { $stub =~ tr|:|/|; }
if ($win32) { $stub =~ tr|\\|/|; }

# Turn the absolute path into a relative path inside the CVS tree
$stub =~ s|.*mozilla/?||;
  
while (<JARFILE>)
{
  # Tidy up the line
  chomp;
  s/^[\s|\+]+//; # Some lines have a + sign before them
  s/\s+$//;

  # There's loads of things we aren't interested in
  next if m/:/;              # e.g. "comm.jar:"
  next if m/^\s*#/;          # Comments
  next if m/^$/;             # Blank lines
  next if m/\.gif\)\s*$/i;   # Graphics
  next if m/\.png\)\s*$/i;
  next if m/\.jpe?g\)\s*$/i;
  next if ($stub =~ m|/win/|)  and !$win32; 
  next if ($stub =~ m|/unix/|) and !$unix; 
  next if ($stub =~ m|/mac/|)  and !$macos; 

  # Split up the common format, which is:
  # skin/fred/foo.xul (xpfe/barney/wilma/foo.xul)
  m/(.*)\s+\((.*)\)/;
  my $chromefile = $1;
  my $cvsfile = $2;

  # Deal with those jar.mns which have just a single line,
  # implying that the file is in the current directory.
  if (!$1 || $1 eq "")
  {
    $chromefile = $_;
    $cvsfile = File::Basename->basename($_);
  }

  # Convert to platform-specific separator for the chrome version.
  # This permits easy grepping of the file for a given path.
  if ($macos) { $chromefile =~ tr|/|:|; }
  if ($win32) { $chromefile =~ tr|/|\\|; }

  $cvsfile = File::Spec::Unix->catfile($stub, $cvsfile);
  
  print BIGLIST "$chromefile ($cvsfile)\n";
}

mozUnlock($lockfile) unless $nofilelocks;

exit(0);

