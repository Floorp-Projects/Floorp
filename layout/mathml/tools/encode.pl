#!/bin/perl

#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is Mozilla MathML Project.
#
# The Initial Developer of the Original Code is The University Of
# Queensland.  Portions created by The University Of Queensland are
# Copyright (C) 2001 The University Of Queensland.  All
# Rights Reserved.
#
# Contributor(s):
#   Roger B. Sidje <rbs@maths.uq.edu.au> - Original Author
#

# RBS - Last Modified: March 14, 2001.

#
# Usage: perl encode.pl font-encoding-table.html [-t truetype | type1]
#

require 'getopts.pl';
our($opt_h, $opt_f, $opt_t);
#
require 'mathfont.pl';
my($DEBUG) = 1;

# where is the MathML DTD file?
my($DTDfile) = "C:\\Mozilla\\src\\mozilla\\layout\\mathml\\content\\src\\mathml.dtd";

# where to load the PUA file?
my($PUAfile) = "C:\\Mozilla\\src\\mozilla\\layout\\mathml\\base\\src\\mathfontPUA.properties";

# where to save the PUA file if new assignments to the PUA are made?
my($newPUAfile) = $PUAfile;

# get the basename of the script
my($progname) = $0 =~ /([^\/\\]+)$/;

sub usage
{
my($comment) = @_;
my($usage) = <<__USAGE__;
Usage: perl $progname [-h] -f fontfile.html [-t truetype | type1]

 Purpose
   This script takes in input a Mozilla's MathFont Encoding Table and
   outputs the data needed to support the font, i.e., the data for the
   ucvmath module and the data for the MathFont Property File.

 Options
   -h (help, this message)

   -f fontfile.html
      The file that contains the font's encoding table. To see examples,
      launch 'view-source' on the .html files of the default
      fonts at: http://www.mozilla.org/projects/mathml/fonts/encoding/
      These .html files are *exactly* the files passed to this script
      with this -f option. For example, to get the data currently used
      by Mozilla for the Math1 font, this script was executed as:

      encode.pl -f math1.html

      The script parses the .html file to extract the mapping enclosed in
       <!--cmap-->
         ...
       <!--/cmap-->

      Not only does this ease maintenance, but it also means that the website
      reflects the latest updates. Note however that the MathFont Property
      Files are also hand-tuned to refine the results and to add further
      customization. Such refinements should not be done on the files generated
      by this script directly (see below). Otherwise you will overwrite and
      thus lose your changes if you run the script again.

   -t encodingtype
      This option should not be used if the TrueType and Type1 versions
      have the same encoding. If there are different encodings, use
      -t truetype : to process the TrueType encoding table inside the file
      -t type1    : to process the Type1 encoding table inside the file

 OUTPUT:
 Given the fontfile.html on input, three files are created on output:

 1. a file called fontname-ucvmath.txt : this is the data to pass to
    "umaptable" for the ucvmath module.

    For example, running "encode.pl -f math1.html" produces math1-ucvmath.txt.
    Then, running "umaptable -uf < math1-ucvmath.txt > mathematica1.uf" will
    produce the resulting file that is currently in mozilla/intl/uconv/ucvmath.
    "umaptable" is a standalone C program that can be built from 
    mozilla/intl/uconv/tools and the executable copied where you want.

 2. fontname-properties.txt : this is the data for the MathFont Property File.
    Continuing with the example of Math1, a file called math1-properties.txt
    will be created. Then after hand-tuning (if necessary), the file is copied
    to its final destination:
    mozilla/layout/mathml/base/src/mathfontMath1.properties

 3. fontname-encoding.html : the is a prettified output that shows the
    resulting mappings. For the example of Math1, this is the file that is at:
    http://www.mozilla.org/projects/mathml/fonts/encoding/math1-encoding.html

 4. Furthermore, since new assignments can be made to the PUA during
    processing, the mathfontPUA.properties file is updated with any new
    assignments that may have been made.
__USAGE__
  $comment = "\nBad usage: $comment\n" if $comment;
  die "$comment\n$usage";
}

#
&usage("Missing arguments") if !&Getopts('hf:t:');
&usage("") if $opt_h; # help option

# The file that contains MathFont Encoding Table(s)
my($encoding_file) = $opt_f;
&usage("Missing file") unless $opt_f;
$encoding_file = "encoding\\" . $encoding_file;

# The encoding type should be:
# . "" (empty), if there is only one encoding for both TrueType and Type1
# . "truetype", to process the TrueType encoding table inside the file
# . "type1", to process the Type1 encoding table inside the file
my($encoding_type) = $opt_t;
&usage("Unexpected type") unless $encoding_type eq "" || 
                                 $encoding_type eq "type1" || 
                                 $encoding_type eq "truetype";

# Processing starts here
#######################################################################

# global variables -- in capitals

# $UNIDATA{$unicode} holds a whitespace-separated list of entities and
# annotated unicode points that all resolve to that $unicode point.
# %UNIDATA is declared in mathfont.pl

# $GLYPHDATA{$glyph} holds a whitespace-separated list of entities and
# annotated unicode points that all resolve to that $glyph index.
my(%GLYPHDATA);

# $MAP{$glyph} holds the ultimate resolved unicode point of the $glyph
# (could be a PUA value).
my(%MAP);

# load current assignments to the PUA
&load_PUA($PUAfile);

# parse the supplied encoding data
my($fontfamily) = &parse_mapping_table($encoding_file, $encoding_type);

# resolve all mappings
&generate_mapping_data($fontfamily);

# save the PUA to preserve any new assignments
&save_PUA($PUAfile, $newPUAfile);

# dump results
&output_stretchy_data($fontfamily, $encoding_type);
&output_mapping_data($fontfamily, $encoding_type);
&output_encoding_map($fontfamily, $encoding_type);

exit(0);



#########################################################################
# parse_mapping_table:
#
# Parse a file containing the mapping between glyph indices and
# unicode points. The file is formatted as:
# <!--cmap:FontFamilyName(:truetype|:type1)-->
#  0xNN  0xNNNN  #optional trailing comment
#  0xNN  0xNNNN:0
#  0xNN  0xNNNN:1
# -0xNN
#  0xNN  0xNNNN:T, 0xABCD
#  0xNN  0xNNNN:G, 0xABCD
#  0xNN >PUA #required comment with &entity;
#  ...etc
# <!--/cmap:FontFamilyName(:truetype|:type1)-->
#
# Note that there is an enclosing: <cmap> ... </cmap> with the *required*
# FontFamilyName and the optional type (exactly 'truetype' or 'type1').
#
# The first field may contain a dash '-', in which case the line is skipped.
# The second field is the glyph index.
# The third field is a comma-delimited list of its associated unicode points
# with annotations w.r.t their applicability ('T', 'L', 'M', 'B', 'R', '0'-'9').
#
# Partial glyphs can apply to different chars. The annotation for '0' is
# optional, i.e., it is assumed if no other annotation has been specified.
# The list can run over many lines provided the preceding line ends
# with a comma.
#
# If the keyword '>PUA' is present, or the unicode point is in plane 1,
# a PUA code is assigned, and is associated with the required '&entity;'
# that must be provided in the comment field.
#########################################################################
sub parse_mapping_table
{
  my($file, $type) = @_;

  local(*FILE);
  open(FILE, $file) || die "Cannot find $file\n";
  my($data) = join("", <FILE>);
  close(FILE);

  my($fontfamily);
  $type = ':' . $type if $type;
  if ($data =~ m|<!--cmap:([^>]+?)$type-->(.+?)<!--/cmap:([^>]+?)$type-->|s) {
    die "ERROR *** Bad mapping: mistmatching tags $type start:$1 end:$3" unless $1 eq $3;
    ($fontfamily, $data) = ($1, $2);
    die "ERROR *** No specified font type $1" if $fontfamily =~ /:/;
  }
  else {
    die "ERROR *** Bad mapping: data must be enclosed in the <cmap> tag";
  }

  my($isContinuation) = 0;
  my($line, $glyph, $comment, $oldline, $oldcomment);

  my (@lines) = split("\n", $data);
  foreach $line (@lines) {
    # skip bogus lines
    next if $line =~ m/^-/;

    # remove leading and trailing whitespace
    $line =~ s/^\s+//; $line =~ s/\s+$//;

    # cache comments in case the keyword '>PUA' is present
    $comment = ($line =~ m/#(.*)/) ? $1 : "";
    $line =~ s/\s*#.*//;

    # see if this is the continuation of a longer line
    if ($isContinuation) {
      $line = $oldline . ' ' . $line;
      $comment = $oldcomment . ' ' . $comment;
      $isContinuation = 0;
    }

    # if the line ends with a comma, the next line is a continuation
    if ($line =~ m/,$/) {
      $oldline = $line;
      $oldcomment = $comment;
      $isContinuation = 1;
      next;
    }

    # get the mapping on the line
    next unless ($line =~ m/^(0x..)\s+(.+)$/);
    ($glyph, $data) = ($1, $2);

    # see if this a '>PUA' or plane 1 character
    if (($data eq '>PUA') || ($data =~ m/^0x1..../)) {
      my($entitylist) = '';
      while ($comment =~ /&(.+?);/g) {
        $entitylist .= ' ' . $1;
      }
      $entitylist =~ s/^\s//;
      die "ERROR *** No entities found: $line" unless $entitylist;
      $GLYPHDATA{$glyph} = $entitylist;

      # continue to next line
      next;
    }

    # skip bogus lines where uncertainties still remain
    $data .= ' ';
    next unless $data =~ /^0x....\s*[, ]/ || $data =~ /^0x....:[TLMBRG0-9]\s*[, ]/;
    chop($data);

    # convert from comma-delimited to whitespace-delimited
    $data =~ s/\s*,\s*/ /g;

    # add explicit 0 at size0
    $data =~ s/(0x....) /$1:0 /;
    $data =~ s/(0x....)$/$1:0/;

    # $GLYPHDATA{$glyph} is a list of referrers to that glyph
    $GLYPHDATA{$glyph} = $data;
  }

  # check for correctness
  &verify_mapping_table();

  return $fontfamily;
}

#########################################################################
# verify_mapping_table:
#
# helper to check that some common mistakes were not made when
# setting up the encoding table.
#########################################################################
sub verify_mapping_table
{
  my($glyph, $entry, $tmp);
  foreach $glyph (keys %GLYPHDATA) {
   my(@data) = split(' ', $GLYPHDATA{$glyph});
    foreach $entry (@data) {
      # verify that $entry wasn't listed twice on distinct lines
      foreach $tmp (keys %GLYPHDATA) {
        next if $tmp eq $glyph;
        next unless $GLYPHDATA{$tmp} =~ /\b$entry\b/;
        die "ERROR *** Duplicate: $glyph $GLYPHDATA{$glyph} vs. $tmp $GLYPHDATA{$tmp}";
      }
      # verify that $entry wasn't listed twice on the same line
      $tmp = $GLYPHDATA{$glyph};
      $tmp =~ s/\b$entry\b//;
      next unless $tmp =~ /\b$entry\b/;
      die "ERROR *** Duplicate: $glyph $GLYPHDATA{$glyph}";
    }
  }
}

#########################################################################
# generate_mapping_data:
#
# This routine resolves the unicode points (PUA and non-PUA) of all 
# elements. Upon completion, the assignments for all entities, annotated
# unicode points, and glyph indices are known.
#########################################################################
sub generate_mapping_data
{
  my($font) = @_;

  my($glyph);
  foreach $glyph (sort keys %GLYPHDATA) {
    # $GLYPHDATA{$glyph} is a list of referrers to that glyph
    # see if one of the referrers is already resolved
    my($entry);
    my($unicode) = '';
    my(@data) = split(' ', $GLYPHDATA{$glyph});
    foreach $entry (@data) {
      # see if this entry is the unicode associated to size0
      if ($entry =~ /(0x....):0/) {
        $unicode = $1;
        last;
      }
      # see if this entry is already associated to a unicode point
      $unicode = get_unicode($entry);
      last if $unicode;
    }
    # if we found one entry that was already resolved, make
    # everybody on the list resolve to that unicode point
    if ($unicode) {
      foreach $entry (@data) {
        next if $entry =~ /0x....:0/;
        if (defined $UNIDATA{$unicode}) {
          $UNIDATA{$unicode} .= " $entry"
            unless $UNIDATA{$unicode} =~ /\b$entry\b/;
        }
        else {
          $UNIDATA{$unicode} = $entry;
        }
      }
    }
    else {
      # make a new assignment to the PUA for this encoding point
      $unicode = &assign_PUA($GLYPHDATA{$glyph});
    }
    # now, we know the unicode point of the glyph
    die "ERROR *** Duplicate $glyph" if defined $MAP{$glyph};
    $MAP{$glyph} = $unicode;
  }

  # check that all went well
  verify_mapping_data();
}

#########################################################################
# verify_mapping_data:
#
# helper to check the validity of the resolved mapping.
#########################################################################
sub verify_mapping_data
{
  my($unicode, $entry, $tmp);
  foreach $unicode (keys %UNIDATA) {
    my(@data) = split(' ', $UNIDATA{$unicode});
    foreach $entry (@data) {
      # verify that $entry wasn't assigned twice on distinct slots
      foreach $tmp (keys %UNIDATA) {
        next if $tmp eq $unicode;
        # we don't care about different mappings outside the PUA because
        # these are not kept in the mathfontPUA.properties file
        next unless &is_pua($tmp) && &is_pua($unicode);
        next unless $UNIDATA{$tmp} =~ /\b$entry\b/;
        # if we reach here, different mappings in the PUA were found
        # for the same annotated code point, something is wrong
        die "ERROR *** Duplicate: $unicode $UNIDATA{$unicode} vs. $tmp $UNIDATA{$tmp}";
      }
      # verify that $entry wasn't listed twice on the same slot
      $tmp = $UNIDATA{$unicode};
      $tmp =~ s/\b$entry\b//;
      next unless $tmp =~ /\b$entry\b/;
      die "ERROR *** Duplicate: $unicode $UNIDATA{$unicode}";
    }
  }
}

sub is_pua
{
  my($value) = @_;
  if ($value =~ s/^0x//) {
    my($numeric) = hex($value);
    return 1 if $numeric >= 0xE000 && $numeric <= 0xF8FF;
  }
  return 0;
}

#########################################################################
# output_mapping_data:
#
# output the mapping data that goes in the ucvmath module
#########################################################################
sub output_mapping_data
{
  my($font, $type) = @_;

  $type = "-$type" if $type;
  my($file) = "$font-ucvmath$type.txt";
  $file =~ s/ //g;
  $file = lc($file);
  print "Saving mapping data of $font in: $file\n";

  local(*FILE);
  open(FILE, ">$file") || die "Cannot open $file\n";
  my($glyph);
  foreach $glyph (sort keys %MAP) {
    print FILE "$glyph $MAP{$glyph}\n";
  }
  close(FILE);
}

#########################################################################
# output_stretchy_data:
#
# output the list of stretchy data in the compact format expected by
# the MathFont Property File.
#########################################################################
sub output_stretchy_data
{
  my($font, $type) = @_;

  $type = "-$type" if $type;
  my($file) = "$font-properties$type.txt";
  $file =~ s/ //g;
  $file = lc($file);

  print "Saving properties of $font in: $file\n";
  &load_DTD($DTDfile);

  my($pattern);
  my(@patterns) = qw {
    (\S+):[TL]
    (\S+):M
    (\S+):[BR]
    (\S+):G
  };

  my($unicode, $glyph, $label);

  # construct the _transpose_ of the GLYPHDATA table so that upon
  # completion, $table{unicode} = list of its associated glyph
  # indices with the annotations flipped on the other side
  my(%table);
  foreach $glyph (%GLYPHDATA) {
    my(@data) = split(' ', $GLYPHDATA{$glyph});
    foreach $unicode (@data) {
      # the '0x' prefix is not kept here
      next unless $unicode =~ m|0x(....):(.)|;
      ($unicode, $label) = ($1, $2);
      if (defined $table{$unicode}) {
        $table{$unicode} .= " $glyph:$label";
      }
      else {
        $table{$unicode}  = "$glyph:$label";
      }
    }
  }

  # now, replace the glyph indices with their resolved unicode points
  my($parts, $sizes, $comment);

  local(*FILE);
  open(FILE, ">$file") || die "Cannot open $file\n";
  foreach $unicode (sort keys %table) {
    my($isMutable) = 0;

    # partial glyphs
    $parts = '';
    foreach $pattern (@patterns) {
      if ($table{$unicode} =~ m/$pattern/) {
      	$glyph = $1;
        $parts .= &indirect_pua($MAP{$glyph});
        $isMutable = 1;
      }
      else {
        $parts .= '\uFFFD';
      }
    }

    # glyphs of larger sizes
    my($sizes) = "\\u$unicode"; # size0 is the char itself 
    $label = '1';
    while ($table{$unicode} =~ m/(\S+):$label/) {
      $glyph = $1;
      $sizes .= &indirect_pua($MAP{$glyph});
      ++$label;
      $isMutable = 1;
    }

    # ignore this character if it is not mutable
    next unless $isMutable;

    # entry for the list of glyphs
    $comment = $ENTITY{"0x$unicode"};
    if ($DEBUG) {
      print FILE "\\u$unicode = $parts$sizes $comment\n";
    }
    else {
      print FILE "$comment\n\\u$unicode = $parts$sizes\n";
    }
  }
  close(FILE);
}

sub indirect_pua
{
  my($unicode) = @_;

  # see if this code is the PUA
  my($numeric) = hex($unicode);
  die "ERROR *** Forbidden mapping" if $numeric == 0xF8FF;
  if ($numeric >= 0xE000 && $numeric <= 0xF8FF) {
    # return the flag to indicate to lookup in the PUA
    return '\uF8FF';
  }
  # re-use the existing unicode point
  $unicode =~ s/0x/\\u/;
  return $unicode;
}

#########################################################################
# output_encoding_map:
#
# output the encoding map in html format for visual comparison with the
# graphical character map.
#########################################################################
sub output_encoding_map
{
  my($font, $type) = @_;
 $type = "-$type" if $type;

  my($shorttype) = '';
  $shorttype = '-t1' if $type eq '-type1';
  $shorttype = '-ttf' if $type eq '-truetype';

  my($file) = "$font$shorttype-encoding.html";
  $file =~ s/ //g;
  $file = lc($file);
  print "Saving visual encoding result of $font in: $file\n";

  local(*FILE);
  open(FILE, ">$file") || die "Cannot open $file\n";

  # header
  print FILE "<html>\n"
           . "<head>\n"
           . "  <title>$font$type - Visual Encoding Result</title>\n"
           . "  <style type='text/css'>.glyph {font-family: $font} </style>\n"
           . "</head>\n"
           . "<body>\n"
           . "<h2>$font$type - Visual Encoding Result</h2>\n\n";
  # column indices
  print FILE "<table border='1' cellpadding='4'>\n<tr align='center'><td> </td>";
  my($i, $j);
  for ($j = 0; $j <= 15; ++$j) {
    print FILE sprintf("<td bgcolor='silver'>%X         </td>", $j);
  }
  print FILE "</tr>\n";
  # cmap array
  for ($i = 0; $i <= 15; ++$i) {
    print FILE sprintf("<tr align='center'><td>%X </td>", $i);
    for ($j = 0; $j <= 15; ++$j) {
      my($glyph) = sprintf("0x%X%X", $i, $j);
      if (defined $GLYPHDATA{$glyph}) {
        my($unicode) = $MAP{$glyph};
        my($data) = $GLYPHDATA{$glyph};
        $unicode =~ s|0(x....)|&#$1;|;
        $data =~ s| |<br />|g;
        $data =~  s|(0x....):(.)|<font color='darkblue'>$1</font>:<font color='brown'>$2</font>|g;
        print FILE "<td>"
                 . "<font size='-1' color='#666666'>$MAP{$glyph}</font><br />"
                 . "<span class='glyph'>$unicode</span><br />"
                 . $data
                 . "</td>";
      }
      else {
        print FILE '<td>&nbsp;</td>';
      }
    }
    print FILE "</tr>\n";
  }
  # footer
  print FILE "</table>\n\n</body>\n</html>\n";

  close(FILE);
}

#########################################################################
# load_DTD:
#
# load the mathml DTD so that we can comment outputs with entities
#########################################################################
sub load_DTD
{
  my($file) = @_;
  local(*FILE);
  open(FILE, $file) || die "Cannot open $file\n";
  while (<FILE>) {
    while (/<!ENTITY\s+(\S+)\s+'&#x([^>]+);'>/g) {
      my($entity, $unicode) = ($1, "0x$2");
      $entity = chr(hex($unicode)) if hex($unicode) <= 0xFF;
      if (defined $ENTITY{$unicode}) {
        $ENTITY{$unicode} .= ", $entity";
      }
      else {    
        $ENTITY{$unicode}  = "# $entity";
      }
    }
  }
  close(FILE);
}
