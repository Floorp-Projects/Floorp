#!/usr/bin/env perl

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This tool is used to prepare a list of valid language subtags from the
# IANA registry (http://www.iana.org/assignments/language-subtag-registry).

# Run as
#
#   perl genLanguageTagList.pl language-subtag-registry > gfxLanguageTagList.cpp
#
# where language-subtag-registry is a copy of the IANA registry file.

use strict;

my $timestamp = gmtime();
print <<__END;
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
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Kew <jfkthame\@gmail.com>
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

/*
 * Derived from the IANA language subtag registry by genLanguageTagList.pl.
 *
 * Created on $timestamp.
 *
 * * * * * This file contains MACHINE-GENERATED DATA, do not edit! * * * * *
 */

__END

my $isLanguage = 0;

while (<>) {
  # strip leading/trailing whitespace, if any
  chomp;
  s/^\s+//;
  s/\s+$//;

  # assume File-Date precedes the actual list;
  # record the date, and begin assignment to an array of valid tags
  if (m/^File-Date:\s*(.+)$/) {
    print "// Based on IANA registry dated $1\n\n";
    print "static const PRUint32 sLanguageTagList[] = {";
    next;
  }

  if (m/^%%/) {
    $isLanguage = 0;
    next;
  }

  # we only care about records of type 'language'
  if (m/^Type:\s*(.+)$/) {
    $isLanguage = ($1 eq 'language');
    next;
  }

  # append the tag to our string, with ";" as a delimiter
  if ($isLanguage && m/^Subtag:\s*([a-z]{2,3})\s*$/) {
    my $tagstr = $1;
    print "\n  TRUETYPE_TAG(",
      join(",", map { $_ eq " " ? " 0 " : "'" . $_ . "'" } split(//, substr($tagstr . "  ", 0, 4))),
      "), // ", $tagstr;
    next;
  }

  if ($isLanguage && m/^Description:\s*(.+)$/) {
    print " = $1";
    $isLanguage = 0; # only print first Description field
    next;
  }
}

# at end of file, terminate our assignment to the array of tags
print <<__END;

  0x0 // end of language code list
};

/*
 * * * * * This file contains MACHINE-GENERATED DATA, do not edit! * * * * *
 */
__END
