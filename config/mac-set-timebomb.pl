#!/usr/local/bin/perl5 -w
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
# The Original Code is Mozilla Communicator client code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
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

my $progname = $0;
my $contents;

# This is the preferences file that gets read and written.
# So run this script with src/mozilla/build/ as the current directory.
#
my $prefs = ":mozilla:modules:libpref:src:init:all.js";


# from noah, who should be shot
sub ctime {
  local (@weekday, @month, $time, $TZ);
  local ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst);

  @weekday = ("Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat");
  @month = ("Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec");

  $time = (length (@_) > 0)? $_[0] : time() ;
  $TZ = defined($ENV{'TZ'}) ? ( $ENV{'TZ'} ? $ENV{'TZ'} : 'UTC' ) : '';
  ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) =
      ($TZ eq 'UTC') ? gmtime($time) : localtime($time);
  if ($TZ =~ /^([^:\d+\-,]{3,})([+-]?\d{1,2}(:\d{1,2}){0,2})([^\d+\-,]{3,})?/o)
    {
      $TZ = $isdst ? $4 : $1;
    }

  if ($TZ ne "") { $TZ .= " "; }
  $year += ($year < 70) ? 2000 : 1900;
  return sprintf ("%s %s %02d %02d:%02d:%02d %s%4d",
                  $weekday[$wday], $month[$mon], $mday, $hour, $min, $sec,
                  $TZ, $year);
}

sub read_file {
    $contents = "";
    open(IN, "<$prefs");
    while (<IN>) {
	$contents .= $_;
    }
    close(IN);
}

sub write_file {
    open(OUT, ">$prefs");
    print OUT $contents;
    close(OUT);
#     print STDERR "$progname: wrote $prefs\n";
}

sub get_pref {
    my ($lvalue) = @_;
    $_ = $contents;
    die ("$lvalue unset?\n") unless m@^config\("$lvalue","(.*)"\);@m;
    return $1;
}

sub set_pref {
    my ($lvalue,$rvalue) = @_;
    $_ = $contents;
#    die("$lvalue unset?\n") unless (m@^(pref|config)\("$lvalue",@m);
    die("$lvalue unset?\n")
	unless s@^(pref|config)(\("$lvalue",)(.*)(\).*)$@$1$2$rvalue$4@m;
    $contents = $_;
}

# no longer used?
sub set_indirected_bomb {
    my ($bomb) = @_;
    my ($secret) = get_pref("timebomb.relative_timebomb_secret_name");
    set_pref($secret, $bomb);
}

sub time_diff {
    use Time::Local;
    my($time, $result) = @_;
    my $diff = 2082830400 - (timegm(localtime) - time);
    return $result =~ /mac/ ?
        $time + $diff : $time - $diff;
}

sub set_bomb {
    my ($warning_days, $bomb_days) = @_;

#    die("warning_days ($warning_days) must be greater than 0.")
#	unless ($warning_days > 0);

    die("bomb_days ($bomb_days) must be greater than 0.")
	unless ($bomb_days > 0);
    die("warning_days ($warning_days) must be less than " .
	"bomb_days ($bomb_days)\n")
	unless ($warning_days < $bomb_days);

    my $mactime = time;

#   MacPerl stores date and times from 1904 instead of 1970
#   Conversion routine thanks to Chris Nandor (pudge@pobox.com)

    $now = time_diff($mactime, 'unix');

    my $bomb = $now + ($bomb_days * 24 * 60 * 60);
    my $warn = $now + ($warning_days * 24 * 60 * 60);

    set_pref("timebomb.expiration_time", $bomb);
    set_pref("timebomb.warning_time", $warn);
    set_pref("timebomb.relative_timebomb_days", -1);
    set_pref("timebomb.relative_timebomb_warning_days", -1);

    set_indirected_bomb(0);

#     print STDERR sprintf("%s: timebomb goes off in %2d days (%s)\n",
# 			 $progname, $bomb_days, ctime($bomb));
#     print STDERR sprintf("%s:  warning goes off in %2d days (%s)\n",
# 			 $progname, $warning_days, ctime($warn));
}

sub main {
    my ($warning_days, $bomb_days) = @_;
    die ("usage: $progname [ days-until-warning [ days-until-timebomb ]]\n")
	if ($#_ >= 2);

    if ($#_ == 0) {
	$bomb_days = $warning_days;
	$warning_days = -1;
    }
    if (!$bomb_days || $bomb_days <= 0) {
	$bomb_days = 30;
    }
    if ($warning_days < 0) {
	$warning_days = $bomb_days - int($bomb_days / 3);
	if ($warning_days < $bomb_days - 10) {
	    $warning_days = $bomb_days - 10;
	}
    }

    read_file();
    set_bomb($warning_days, $bomb_days);
    write_file();
    return 0;
}

exit(&main(@ARGV));


