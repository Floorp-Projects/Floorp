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
# The Original Code is Mozilla MathML Project.
#
# The Initial Developer of the Original Code is
# The University Of Queensland.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Roger B. Sidje <rbs@maths.uq.edu.au> - Original Author
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

# Utilities to save and retrieve the PUA
# RBS - Last Modified: March 20, 2001.

# $UNIDATA{$unicode} holds a whitespace-separated list of entities and
# annotated unicode points that all resolve to that $unicode point

#######################################################################
# Initialize the starting point from where we want to make assignments
# to the PUA (0xE000 - 0xF8FF).
# Issues:
# De-facto zones owned by others: 
# . Mozilla: 0xF780 - 0xF7FF for the "User Defined" font (bug 6588)
# . Apple: 0xF8A1 - 0xF8FF for Apple specific chars
# . Microsoft: 0xE000 - 0xEDE7 for CJK's EUDCs (i.e., Chinese/Japanese/Korean's
#              end user defined characters)
# . Linux: 0xF000 - 0xF8FF for DEC VT100 graphics chars and console chars
# Hopefully we won't get into troubles within the <math>...</math> environment!
$UNIDATA{'PUA'} = '0xEEEE';

#########################################################################
# load_PUA:
#
# To avoid collisions, populate the PUA with code points that are already
# taken (or reserved) so that we don't pick them up again
#########################################################################
sub load_PUA
{
  my($mathfontPUAproperties) = @_;

  local(*FILE);
  my($pua, $owner);

  open(FILE, $mathfontPUAproperties) || die "Cannot open $mathfontPUAproperties\n";
  while (<FILE>) {
    if (m/^\\u(\S+)\s*=\s*\\u(\S+)/) {
      # 1) the annotated unicode point is on the left hand side
      # 2) the assignment to the PUA is on the right hand side
      ($owner, $pua) = ("0x$1", "0x$2");
      # convert dot-to-colon for compatibility with the encoding file
      $owner =~ s/\./:/;
      if (defined $UNIDATA{$pua}) {
        $UNIDATA{$pua} .= ' ' . $owner;
      }
      else {
        $UNIDATA{$pua} = $owner;
      }
    }
    elsif (m/^#>\\u(\S+)\s*=\s*([^#]*)/) {
      # 1) the assignment to the PUA is on the LHS, 2) the entity list is
      # on the RHS, the list can be empty (a _special case_ where it is not
      # yet known if there is an entity for that PUA code point)
      ($pua, $owner) = ("0x$1", $2);
      $owner =~ s/\s*,\s*/ /g; # convert to whitespace-separated
      $owner =~ s/\s+$//;
      # don't bother with empty owner
      next unless $owner;
      if (defined $UNIDATA{$pua}) {
        $UNIDATA{$pua} .= ' ' . $owner
      }
      else {
        $UNIDATA{$pua} = $owner;
      }
    }
  }
  close(FILE);
}

#########################################################################
# save_PUA:
#
# When we are done with the PUA, save it to preserve our changes
#########################################################################
sub save_PUA
{
  my($old_mathfontPUAproperties, $new_mathfontPUAproperties) = @_;

  # first, load the PUA again because we want to preserve the same
  # ordering, comments, etc, that are there -- like in a context-diff...
  local(*FILE);
  open(FILE, $old_mathfontPUAproperties) || die "Cannot open $old_mathfontPUAproperties\n";
  my(@lines) = <FILE>;
  close(FILE);

  my(%saved);
  my($line, $pua, $owner);
  open(FILE, ">$new_mathfontPUAproperties") || die "Cannot open $new_mathfontPUAproperties\n";

  foreach $line (@lines) {
    $line =~ s/\cM\n/\n/; # dos2unix end of line
    # update section 1
    if ($line =~ m/^\\u(\S+)\s*=\s*\\u(\S+)/) {
      # mark this data as saved
      $saved{"\\u$1"} = "\\u$2";
    }
    elsif ($line =~ m/^#>EndSection1/) {
      # output the new assignments that have been made
      foreach $pua (sort keys %UNIDATA) {
        # sanity check: the only foreign key should be the special 'PUA'
        next if $pua eq 'PUA';
        # assignments outside the PUA are not saved
        my($numeric) = hex($pua);
        next unless ($numeric >= 0xE000 && $numeric <= 0xF8FF);
        $tmp = $UNIDATA{$pua};
        die("something is wrong somewhere: $pua") unless $pua =~ m/^0x(....)$/;
        $pua = "\\u$1";
        foreach $owner (split(' ', $tmp)) {
          # skip entries that are intended for section 2
          next unless $owner =~ m/^0x(....:.)$/;
          $owner = "\\u$1";
          # convert colon-to-dot for compatibility with the property file
          $owner =~ s/:/\./;
          # skip entries that are already saved
          next if defined $saved{$owner};
          # mark this data as saved
          $saved{$owner} = $pua;
          print FILE "$owner = $pua\n";
        }
      }
    }
    # update section 2
    elsif ($line =~ m/^#>\\u(\S+)\s*=\s*([^#]*)(.*)/) {
      ($pua, $owner, $tmp) = ($1, $2, $3);
      # the owner list may have expanded, update and keep the comment
      $owner = $UNIDATA{"0x$pua"};
      $tmp = ' ' . $tmp if $tmp;
      # remove references handled in section1, there could be a mix of owners
      # no check for empty owner to preserve what was already saved
      $owner =~ s/\s*0x....:.//g;
      $owner =~ s/^\s+//; $owner =~ s/\s+$//;
      $owner =~ s/ /, /g; # saved as comma-separated
      $line = "#>\\u$pua = $owner$tmp\n";
      # mark this data as saved
      $saved{"\\u$pua"} = $owner;
    }
    elsif ($line =~ m/^#>EndSection2/) {
      # output the new assignments that have been made
      foreach $pua (keys %UNIDATA) {
        # sanity check: the only foreign key should be the special 'PUA'
        next if $pua eq 'PUA';
        ($owner, $tmp) = ($1, $2) if $UNIDATA{$pua} =~ /([^#]+)(.*)/;
        die("something is wrong somewhere: $pua") unless $pua =~ m/^0x(....)$/;
        $pua = "\\u$1";
        $owner =~ s/\s+$//;
        $tmp = ' ' . $tmp if $tmp;
        # skip entries that were saved above
        next if defined $saved{$pua};
        # remove references handled in section1, there could be a mix of owners
        my($count) = $owner =~ s/\s*0x....:.//g;
#        if ($count) {
          $owner =~ s/^\s+//;
          next unless $owner;
#        }
        die("something is wrong somewhere: $owner") unless $owner =~ m/^[A-Za-z]/;
        # mark this data as saved
        $owner =~ s/ /, /g; # saved as comma-separated
        $saved{$pua} = $owner;
        print FILE "#>$pua = $owner$tmp\n";
      }
    }
    print FILE $line;
  }
  close(FILE);
}

#########################################################################
# assign_PUA:
#
# all the elements on the given whitespace-separated list are assigned to
# the same slot in the PUA. The list can contain a mix of entities and
# annotated Unicode points.
#########################################################################
sub assign_PUA
{
  my($owners, $comment) = @_;

  # verify that nobody is already associated to a unicode point
  foreach $entry (split(' ', $owners)) {
    foreach (values %UNIDATA) {
      die "ERROR *** Redefinition of $entry" if /\b$entry\b/;
    }
  }

  # get a new slot in the PUA
  my($pua) = $UNIDATA{'PUA'}; # this holds the current slot in the PUA
  my($numeric) = hex($pua);
  while (defined $UNIDATA{$pua}) {
    $pua = sprintf("0x%04X", ++$numeric);
  }
  die "ERROR *** PUA is fulled!!!" unless $numeric <= 0xF8FF;
  $UNIDATA{'PUA'} = $pua; # cache current slot in the PUA

  # all the owners given on the list should resolve to the same code point
  if (defined $UNIDATA{$pua}) {
    $UNIDATA{$pua} .= $owners . ' ' . $UNIDATA{$pua} . $comment;
  }
  else {
    $UNIDATA{$pua} = $owners . $comment;
  }

  return $pua;
}

# lookup the unicode of an entry
sub get_unicode
{
  my($entry) = @_;

  my($unicode);
  foreach $unicode (keys %UNIDATA) {
    return $unicode if $UNIDATA{$unicode} =~ /\b$entry\b/;
  }
  return '';
}

1;
