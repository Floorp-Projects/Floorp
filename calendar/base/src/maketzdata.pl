#!/usr/bin/perl
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
# The Original Code is Oracle Corporation Code.
#
# The Initial Developer of the Original Code is
#   Oracle Corporation.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Vladimir Vukicevic <vladimir.vukicevic@oracle.com>
#   Matthew Willis <lilmatt@mozilla.com>
#   Dan Mosedale <dmose@mozilla.org>
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
#
#
# This script takes a zones.tab file and directory, such as that
# produced from an Olson-format tz database via vzic.  Note that it
# should be used on vzic output WITHOUT the --pure option.  The
# libical distribution includes ics files generated with --pure,
# which results in both huge files and various incompatability
# problems.
#
# vzic - http://dialspace.dial.pipex.com/prod/dialspace/town/pipexdsl/s/asbm26/vzic/
# olson db - ftp://elsie.nci.nih.gov/pub/
#
use File::Basename;

$TZ_PREFIX = "/mozilla.org/20070129_1";

if ($#ARGV != 1) {
  print "Usage: maketzdata.pl zones.tab tzdata.c\n";
  die;
}

$zonetabfile = $ARGV[0];
$zonetabdir = dirname($zonetabfile);
$tzdatafile = $ARGV[1];

open ZONETAB, $zonetabfile || die "Can't open $zonetab";
@zonetab = <ZONETAB>;
close ZONETAB;

open TZDATA, ">" . $tzdatafile || die "Can't open $tzdata for writing";

print TZDATA <<EOF
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Oracle Corporation Code.
 *
 * The Initial Developer of the Original Code is
 *   Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir.vukicevic\@oracle.com>
 *   Matthew Willis <lilmatt\@mozilla.com>
 *   Dan Mosedale <dmose\@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/**
 ** Automagically generated via maketzdata.pl from zones.tab
 ** DO NOT EDIT BY HAND
 **/

typedef struct {
    /* the tzid  for this */
    char *tzid;

    /* latitude and longitude, in +/-HHMMSS format */
    char *latitude;
    char *longitude;

    /* the ics VTIMEZONE component */
    char *icstimezone;
} ical_timezone_data_struct;

static ical_timezone_data_struct ical_timezone_data[] = {
EOF
;

$first = 1;

foreach my $tz (@zonetab) {
  chop $tz;
  if ($first != 1) {
    print TZDATA ",\n";
  }
  ($lat, $long, $name) = split(/ /, $tz);
  my $tzdata = "";
  {
    local $/ = undef;
    open TZFILE, "$zonetabdir/$name.ics" || die "Can't open timezone file $zonetabdir/$name";
    $tzdata = <TZFILE>;
    close TZFILE;
  }

  # escape quotes
  $tzdata =~ s/\"/\\\"/sg;
  $tzdata =~ s/\n/\\n/sg;

  print TZDATA <<EOF
{
	"$TZ_PREFIX/$name",
	"$lat", "$long",
	"$tzdata"
}
EOF
;

  $first = 0;
}

print TZDATA <<EOF
,
{ NULL, NULL, NULL, NULL }
};

// needed for implementing calIICSService.tzIdPrefix
NS_NAMED_LITERAL_CSTRING(gTzIdPrefix, "${TZ_PREFIX}/");
EOF
;

close TZDATA;
