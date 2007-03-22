#!/usr/bin/env perl
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
# Portions created by the Initial Developer are Copyright (C) 1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Stephen Lamm
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

sub usage {
  warn "usage: cvsco.pl <checkout_command>\n"
      ."       <checkout_command> is the entire cvs co command\n"
      ."             (e.g. cvs -q -z3 co SeaMonkeyAll).\n";
}

usage(), die "Error: Not enough arguments\n" if $#ARGV < 0;
usage(), die "Error: Wrong cwd. Must chdir to mozilla/..\n" 
  unless -r 'mozilla/config/cvsco.pl';

$co_command = join ' ', @ARGV;

$logfile     = 'cvslog.txt';
$old_logfile = 'cvslog-old.txt';

sub dblprint {
  print LOG @_;
  print STDERR @_;
}

if (-r $logfile) {
  rename $logfile, $old_logfile;
  print "rename $logfile, $old_logfile\n";
}

open LOG,   ">$logfile";
open CVSCO, "$co_command|";

dblprint "\ncheckout start: ".scalar(localtime)."\n";
dblprint "$co_command | tee cvslog.txt\n";

while (<CVSCO>) {
  dblprint $_;
  push @conflicts, $_ if /^C /;
}

if (@conflicts) {
  print "Error: cvs conflicts during checkout:\n";
  die join('', @conflicts);
}

close(CVSCO) or die "cvs error.\n";
dblprint 'checkout finish: '.scalar(localtime)."\n";

