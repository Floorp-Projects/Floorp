#!/usr/bin/perl -w
#
#  gen-big5hkscs-2001-mozilla.pl
#      a Perl script that generates Big5-HKSCS <-> Unicode
#      conversion tables for Mozilla
#
#  Author (of the original Perl script):
#      Anthony Fok <anthony@thizlinux.com> <foka@debian.org>
#  Copyright (C) 2001, 2002 ThizLinux Laboratory Ltd.
#  License: GNU General Public License, v2 or later.
#
#  This version includes original C source code from
#  glibc-2.2.5/iconvdata/big5hkscs.c by Ulrich Drepper <drepper@redhat.com>
#  Roger So <roger.so@sw-linux.com>
#
#                         First attempt for Qt-2.3.x: 2001-09-21
#                     A working version for Qt-2.3.x: 2001-10-30
#              Ported to glibc-2.2.5 with HKSCS-2001: 2002-03-21
#  Adapted to generate conversion tables for Mozilla: 2002-11-26
#  Adapted to generate conversion tables for Mozilla: 2002-11-30
#                     Cleaned up the script somewhat: 2002-12-04
# Minor revisions for submitting to Mozilla Bugzilla: 2002-12-10
#
#  Notes:
#
#   1. The latest version of this script may be found in:
#          http://www.thizlinux.com/~anthony/hkscs/gen-glibc-big5hkscs.pl
#          http://people.debian.org/~foka/hkscs/gen-glibc-big5hkscs.pl
#      Or, better yet, e-mail me and ask for the latest version.
#
#   2. This script generates data from 3 tables:
#       a. http://www.microsoft.com/typography/unicode/950.txt
#       b. http://www.info.gov.hk/digital21/chi/hkscs/download/big5-iso.txt
#       c. http://www.info.gov.hk/digital21/chi/hkscs/download/big5cmp.txt
#
#      Make sure your big5-iso.txt is the latest HKSCS-2001 version.
#
#   3. [glibc]: I have currently split the ucs_to_big5_hkscs_?[] tables into
#       different areas similar to the way Ulrich and Roger did it,
#       but extended for HKSCS-2001.
#
#   4. [Mozilla]: This script is very quick-and-dirty in some places.
#       Call either gen_mozilla_uf() or gen_mozilla_ut() to generate
#       the appropriate tables for feeding into "fromu" or "tou".
#
#   5. [CharMapML]: The comments regarding TW-BIG5 herein need to be organized.
#       Also, please make sure "$hkscs_mode = 0;" for TW-BIG5 mode.
#       Otherwise, this script would generate a HKSCS table.
#       (Yes, I know, I should clean up this script and make it more modular,
#       and with command-line options or whatnot.  I'll do that later.  :-)
#
#  If you have any questions or concerns, please feel free to contact me
#  at Anthony Fok <anthony@thizlinux.com> or <foka@debian.org>  :-)
#
#  Last but not least, special thanks to ThizLinux Laboratory Ltd. (HK)
#  for their generous support in this work.
#

# 1. UDA3, 0x8840 - 0x8dfe
# 2. UDA2, 0x8e40 - 0xa0fe
# 3. VDA,  0xc6a1 - 0xc8fe

#use Getopt::Std;

my ( %b2u, %u2b, $unicode, $big5, $high, $low, $i, $count );

my $debug = 0;
my $hkscs_mode = 1;
my $kangxi = 0;
my $use_range  = 0;
my $bmp_only  = 1;

#
# Subroutine Declaration
#
sub read_cp950();
sub adjust_radicals();
sub read_hkscs_main();
sub read_hkscs_cmp();
sub post_tuning();
sub gen_charmapml();
sub gen_check_b2u();
sub gen_check_u2b();
sub gen_mozilla_uf();
sub gen_mozilla_ut();
sub gen_glibc();

###########################################################################
#
# Main program
#

# First, read Microsoft's CP950 as base Big5.
read_cp950 ();

# Add mappings to Kangxi Radicals.
# The b2u direction is added only if $kangxi is not null.
adjust_radicals ();

# Then, read the HKSCS table.
# Again, see the $hkscs_mode variable.
read_hkscs_main ();
read_hkscs_cmp () if $hkscs_mode;

post_tuning ();


# Then, choose one of the following:
#gen_charmapml();
gen_mozilla_uf();
#gen_mozilla_ut();
#gen_check_u2b();
#gen_glibc();


# End of program
exit 0;


#############################################################################
#
#  Subroutines
#

sub read_cp950() {
    open( CP950, "950.txt" ) or die;
    my $mode = 0;
    while (<CP950>) {
        s/\r//;
        chomp;
        next if /^$/;
        last if /^ENDCODEPAGE/;

        if (/^DBCSTABLE (\d+)\s+;LeadByte = 0x([0-9a-f]{2})/) {
            $mode = 1;
            ( $count, $high ) = ( $1, $2 );
            $i = 0;
            next;
        }
        if (/^WCTABLE (\d+)/) {
            $mode  = 2;
            $count = $1;
            $i     = 0;
            next;
        }
        next if $mode == 0;

        if ( $mode == 1 ) {
            ( $low, $unicode, $comment ) = split "\t";
            $low     =~ s/^0x//;
            $unicode =~ s/^0x//;
            $big5 = $high . $low;
            $b2u{ uc($big5) } = uc($unicode);
            if ( ++$i == $count ) { $mode = 0; $count = 0; next; }
        }

        if ( $mode == 2 ) {
            ( $unicode, $big5, $comment ) = split "\t";
            $unicode =~ s/^0x//;
            $big5    =~ s/^0x//;
            my $u = hex($unicode);
            my $b = hex($big5);

            $u2b{ uc($unicode) } = uc($big5) unless

              # Skip Microsoft's over-generous (or over-zealous?) mappings
              # "Faked" accented latin characters
              ( $b <= 0xFF and $b != $u )

              # "Faked" Ideographic Annotation ___ Mark
              or ( $u >= 0x3192 and $u <= 0x319F )

              # "Faked" Parenthesized Ideograph ___
              or ( $u >= 0x3220 and $u <= 0x3243 )

              # "Faked" Circled Ideograph ___ except Circled Ideograph Correct
              or ( $u >= 0x3280 and $u <= 0x32B0 and $u != 0x32A3 )

              # ¢F¢G¢D¡¦£g¡M
              or ( $u == 0xA2
                or $u == 0xA3
                or $u == 0xA5
                or $u == 0xB4
                or $u == 0xB5
                or $u == 0xB8 )

              # ¡Â¢w¡ü¡E£»¡²¡Ã¢B¢X¡Ý¡[¡ó¡ò¡ã¡Ê
              or ( $u == 0x0305		# ???
                or $u == 0x2015
                or $u == 0x2016
                or $u == 0x2022
                or $u == 0x2024
                or $u == 0x2033
                or $u == 0x203E		# ???
                or $u == 0x2216
                or $u == 0x2218
                or $u == 0x2263
                or $u == 0x2307
                or $u == 0x2609
                or $u == 0x2641
                or $u == 0x301C
                or $u == 0x3030 )

              # ¡s¡¥¡N
              or ( $u == 0xFF3E or $u == 0xFF40 or $u == 0xFF64 );

            if ( ++$i == $count ) { $mode = 0; $count = 0; next; }
        }
    }
}

sub adjust_radicals() {

    # B5+C6BF - B5+C6D7: Radicals (?)

    # TW-BIG5 drafted by Autrijus uses Kangxi Radicals whenever possible.
    #
    #   Big5-HKSCS tends towards using the character in Unicode CJK Ideographs
    #   Note that HKSCS does not explicitly define
    #       B5+C6CF, B5+C6D3, B5+C6D5, B5+C6D7 (ÆÏ¡BÆÓ¡BÆÕ¡BÆ×),
    #   but do have these characters at B5+FBFD, B5+FCD3, B5+FEC1, B5+90C4,
    #   mapped to U+5EF4, U+65E0, U+7676, U+96B6 respectively.
    #
    #   As for B5+C6CD (ÆÍ), HKSCS maps it to U+2F33 just like TW-BIG5.
    #   However, it also maps B5+FBF4 (ûô) to U+5E7A.
    $b2u{"C6BF"} = "2F02" if $kangxi;
    $u2b{"2F02"} = "C6BF";              # Æ¿
    $b2u{"C6C0"} = "2F03" if $kangxi;
    $u2b{"2F03"} = "C6C0";              # ÆÀ
    $b2u{"C6C1"} = "2F05" if $kangxi;
    $u2b{"2F05"} = "C6C1";              # ÆÁ
    $b2u{"C6C2"} = "2F07" if $kangxi;
    $u2b{"2F07"} = "C6C2";              # ÆÂ
    $b2u{"C6C3"} = "2F0C" if $kangxi;
    $u2b{"2F0C"} = "C6C3";              # ÆÃ
    $b2u{"C6C4"} = "2F0D" if $kangxi;
    $u2b{"2F0D"} = "C6C4";              # ÆÄ
    $b2u{"C6C5"} = "2F0E" if $kangxi;
    $u2b{"2F0E"} = "C6C5";              # ÆÅ
    $b2u{"C6C6"} = "2F13" if $kangxi;
    $u2b{"2F13"} = "C6C6";              # ÆÆ
    $b2u{"C6C7"} = "2F16" if $kangxi;
    $u2b{"2F16"} = "C6C7";              # ÆÇ
    $b2u{"C6C8"} = "2F19" if $kangxi;
    $u2b{"2F19"} = "C6C8";              # ÆÈ
    $b2u{"C6C9"} = "2F1B" if $kangxi;
    $u2b{"2F1B"} = "C6C9";              # ÆÉ
    $b2u{"C6CA"} = "2F22" if $kangxi;
    $u2b{"2F22"} = "C6CA";              # ÆÊ
    $b2u{"C6CB"} = "2F27" if $kangxi;
    $u2b{"2F27"} = "C6CB";              # ÆË
    $b2u{"C6CC"} = "2F2E" if $kangxi;
    $u2b{"2F2E"} = "C6CC";              # ÆÌ
    $b2u{"C6CD"} = "2F33" if $kangxi;
    $u2b{"2F33"} = "C6CD";              # ÆÍ
    $b2u{"C6CE"} = "2F34" if $kangxi;
    $u2b{"2F34"} = "C6CE";              # ÆÎ
    $b2u{"C6CF"} = "2F35" if $kangxi;
    $u2b{"2F35"} = "C6CF";              # ÆÏ
    $b2u{"C6D0"} = "2F39" if $kangxi;
    $u2b{"2F39"} = "C6D0";              # ÆÐ
    $b2u{"C6D1"} = "2F3A" if $kangxi;
    $u2b{"2F3A"} = "C6D1";              # ÆÑ
    $b2u{"C6D2"} = "2F41" if $kangxi;
    $u2b{"2F41"} = "C6D2";              # ÆÒ
    $b2u{"C6D3"} = "2F46" if $kangxi;
    $u2b{"2F46"} = "C6D3";              # ÆÓ
    $b2u{"C6D4"} = "2F67" if $kangxi;
    $u2b{"2F67"} = "C6D4";              # ÆÔ
    $b2u{"C6D5"} = "2F68" if $kangxi;
    $u2b{"2F68"} = "C6D5";              # ÆÕ
    $b2u{"C6D6"} = "2FA1" if $kangxi;
    $u2b{"2FA1"} = "C6D6";              # ÆÖ
    $b2u{"C6D7"} = "2FAA" if $kangxi;
    $u2b{"2FAA"} = "C6D7";              # Æ×
}

sub read_hkscs_main() {

    open( B2U, "<big5-iso.txt" ) or die;
    while (<B2U>) {
        next
          unless
/([[:xdigit:]]{4})\s+([[:xdigit:]]{4})\s+([[:xdigit:]]{4})\s+([[:xdigit:]]{4,5})/;
        ( $big5, $iso1993, $iso2000, $iso2001 ) = ( $1, $2, $3, $4 );

        my $b = hex($big5);

        # For non-HKSCS mode, only take data in the VDA range (?)
        next unless $hkscs_mode

          # Note that we don't go from B5+C6A1-B5+C6FE, but rather only
          # C6A1-C8D3 excluding C6BF-C6D7 (Kangxi Radicals)
          # because C8D4-C8FE are not assigned in TW-BIG5
          # if we are to follow Arphic PL Big-5 fonts.  (To be discussed)
          or
          ( $b >= 0xC6A1 && $b <= 0xC8D3 and !( $b >= 0xC6BF && $b <= 0xC6D7 ) )
          or ( $b >= 0xF9D6 && $b <= 0xF9FE );

        print STDERR
          "B2U, 2000: $big5 redefined from U+$b2u{$big5} to U+$iso2000.\n"
          if $debug
          and defined( $b2u{$big5} )
          and $b2u{$big5} ne $iso2000;

        $b2u{$big5} = $bmp_only ? $iso2000 : $iso2001
          unless !$hkscs_mode
          and $b == 0xF9FE;

        # B5+F9FE is mapped differently in TW-BIG5 and HKSCS, to
        # U+2593 (Dark Shade) and U+FFED (Halfwidth Black Square) respectively.
        # Which is more correct?  I don't know!  (To be discussed)

        print STDERR
          "1993: U+$iso1993 redefined from $u2b{$iso1993} to $big5.\n"
          if $debug
          and defined( $u2b{$iso1993} )
          and $u2b{$iso1993} ne $big5;

        $u2b{$iso1993} = $big5;

        print STDERR
          "2000: U+$iso2000 redefined from $u2b{$iso2000} to $big5.\n"
          if $debug
          and defined( $u2b{$iso2000} )
          and $u2b{$iso2000} ne $big5;

        $u2b{$iso2000} = $big5;

        print STDERR
          "2001: U+$iso2001 redefined from $u2b{$iso2001} to $big5.\n"
          if $debug
          and defined( $u2b{$iso2001} )
          and $u2b{$iso2001} ne $big5;

        $u2b{$iso2001} = $big5;
    }
    close B2U;

}    # read_hkscs_main()


sub read_hkscs_cmp() {

    ###########################################################################
    # Add Big5 compatibility coding...
    #
    # Stephan, here is the code segment that you may want to implement
    # in your convertbig5hkscs2001.pl 
    #
    open( B5CMP, "<big5cmp.txt" ) or die;
    $mode = 0;
    while (<B5CMP>) {
        if (/^=====/) { $mode = 1; next; }
        next if $mode == 0;
        last if $mode == 1 and /^\s+/;
        chomp;
        my ( $big5cmp, $big5 ) = split " ";

        $big5cmp = uc($big5cmp);
        $big5    = uc($big5);
        my $uni    = $b2u{$big5};
        my $unicmp = $b2u{$big5cmp};

        print STDERR
          "Was: U+$unicmp -> $u2b{$unicmp}, $big5cmp -> U+$b2u{$big5cmp}\t"
          if $debug;
        $b2u{$big5cmp} = $uni;
        $u2b{$unicmp}  = $big5;
        print STDERR
          "Now:  U+$unicmp -> $u2b{$unicmp}, $big5cmp -> U+$b2u{$big5cmp}\n"
          if $debug;
    }
    close B5CMP;
}    # read_hkscs_cmp();


sub post_tuning() {

    # And finally, fine-tuning...
    for $i ( 0x00 .. 0x80 ) {
        $big5 = $unicode = sprintf( "%04X", $i );
        $b2u{$big5} = $unicode;
    }

    # Add Euro '£á' (I wonder why this 950.txt doesn't have it.)
    $b2u{"A3E1"} = "20AC";
    $u2b{"20AC"} = "A3E1";

    # Box drawing characters:
    # Align with Big-5E (To be discussed, as it differs from CP950 and HKSCS)
    # (To be discussed)
    if ( !$hkscs_mode ) {
        $u2b{"2550"} = "A2A4";    # Big5: ¢¤	(also B5-F9F9)
        $u2b{"255E"} = "A2A5";    # Big5: ¢¥	(also B5-F9E9)
        $u2b{"2561"} = "A2A7";    # Big5: ¢§	(also B5-F9EB)
        $u2b{"256A"} = "A2A6";    # Big5: ¢¦	(also B5-F9EA)
        $u2b{"256D"} = "A27E";    # Big5: ¢~	(also B5-F9FA)
        $u2b{"256E"} = "A2A1";    # Big5: ¢¡	(also B5-F9FB)
        $u2b{"256F"} = "A2A3";    # Big5: ¢£	(also B5-F9FD)
        $u2b{"2570"} = "A2A2";    # Big5: ¢¢	(also B5-F9FC)
    }

    # "Hangzhou" or "Suzhou" Chinese numerals 10, 20, 30 (¢Ì¢Í¢Î)
    # (To be discussed)
    if ( !$hkscs_mode ) {
        $b2u{"A2CC"} = "3038";
        $u2b{"3038"} = "A2CC";
        $b2u{"A2CD"} = "3039";
        $u2b{"3039"} = "A2CD";
        $b2u{"A2CE"} = "303A";
        $u2b{"303A"} = "A2CE";
    }

    # The character for ethnic group "Yi" (ÂU):
    # (To be discussed)
    $u2b{"5F5E"} = "C255";    # Always add this.
    if ( !$hkscs_mode ) {
        $b2u{"C255"} = "5F5E";
    }

}    # post_tuning()


sub gen_charmapml() {

    ###########################################################################
    #
    #  Codes for generating CharMapML XML file

    print <<EOT;
<?xml version="1.0" encoding="UTF-8" ?>
<!DOCTYPE characterMapping SYSTEM "http://www.unicode.org/unicode/reports/tr22/CharacterMapping.dtd">
EOT

    if ($hkscs_mode) {
        print <<EOT;
<characterMapping id="big5-hkscs-2001" version="1">
 <history>
  <modified version="1" date="2002-11-30">
   Trial version generated from 950.txt + part of big5-iso.txt (HKSCS-2001)
   with Euro added, with CP950's excessive fub (fallbacks uni->big5) removed,
   and with some other manual tweaking.
  </modified>
 </history>
EOT
    }
    else {
        print <<EOT;
<characterMapping id="tw-big5-2002" version="1">
 <history>
  <modified version="1" date="2002-11-30">
   Trial version generated from 950.txt + part of big5-iso.txt (HKSCS-2001)
   with Euro added, with CP950's excessive fub (fallbacks uni->big5) removed,
   and with some other manual tweaking.
  </modified>
 </history>
EOT
    }

    print <<EOT;
 <validity>
  <state type="FIRST" next="VALID" s="0" e="80" max="FFFF"/>
  <state type="FIRST" next="SECOND" s="81" e="FE" max="FFFF"/>
  <state type="SECOND" next="VALID" s="40" e="7E" max="FFFF"/>
  <state type="SECOND" next="VALID" s="A1" e="FE" max="FFFF"/>
 </validity>
 <assignments sub="3F">
EOT
    print "  <!-- One to one mappings -->\n";
    for $unicode ( sort { hex($a) <=> hex($b) } keys %u2b ) {
        $big5 = $u2b{$unicode};
        $u    = hex($unicode);
        next
          unless defined( $b2u{$big5} )
          and $unicode eq $b2u{$big5}
          and
          not( $use_range and !$hkscs_mode and $u >= 0xE000 && $u <= 0xF6B0 );
        printf "  <a u=\"%04X\" ", $u;
        if ( hex($big5) <= 0xFF ) {
            printf "b=\"%02X\"/>\n", hex($big5);
        }
        else {
            printf "b=\"%s %s\"/>\n", substr( $big5, 0, 2 ),
              substr( $big5, 2, 2 );
        }
    }

    print "  <!-- Fallback mappings from Unicode to bytes -->\n";
    for $unicode ( sort { hex($a) <=> hex($b) } keys %u2b ) {
        $big5 = $u2b{$unicode};
        next if defined( $b2u{$big5} ) and hex($unicode) == hex( $b2u{$big5} );
        if ( $unicode eq "F900" ) {
            print "  <!-- CJK Compatibility Ideographs: U+F900 - U+FA6A.\n";
            print
"       These are included in CP950 (Unicode->Big5 direction only).\n";
            print "       Should we include this area in TW-BIG5 or not? -->\n";
        }
        printf "  <fub u=\"%04X\" b=\"%s %s\"/>\n", hex($unicode),
          substr( $big5, 0, 2 ), substr( $big5, 2, 2 );
    }

    my %fbu;
    print "  <!-- Fallback mappings from bytes to Unicode -->\n";
    for $big5 ( sort { hex($a) <=> hex($b) } keys %b2u ) {
        $unicode = $b2u{$big5};
        if ( !defined( $u2b{$unicode} ) or hex($big5) != hex( $u2b{$unicode} ) )
        {
            $fbu{$unicode} = $big5;
        }
    }
    for $unicode ( sort { hex($a) <=> hex($b) } keys %fbu ) {
        $big5 = $fbu{$unicode};
        printf "  <fbu u=\"%04X\" b=\"%s %s\"/>\n", hex($unicode),
          substr( $big5, 0, 2 ), substr( $big5, 2, 2 );
    }

    if ( $use_range and !$hkscs_mode ) {
        print <<EOT;
  <!-- Roundtrip-mappings that can be enumerated
       Note: We can only use the <range> tag for TW-BIG5.
             Big-5E and Big5-HKSCS have assigned characters in these areas,
	     and we will have to use the <a> and <fub> tags instead.
    -->
  <!-- User-Defined Area 1 (UDA1) -->
  <range uFirst="E000" uLast="E310"  bFirst="FA 40" bLast="FE FE" bMin="81 40" bMax="FE FE"/>
  <!-- User-Defined Area 2 (UDA2) -->
  <range uFirst="E311" uLast="EEB7"  bFirst="8E 40" bLast="A0 FE" bMin="81 40" bMax="FE FE"/>
  <!-- User-Defined Area 3 (UDA3) -->
  <range uFirst="EEB8" uLast="F6B0"  bFirst="81 40" bLast="8D FE" bMin="81 40" bMax="FE FE"/>
EOT
    }

    print <<EOT;
 </assignments>
</characterMapping>
EOT

}    # gen_charmapml()

sub gen_check_b2u() {

    ###########################################################################
    #
    #  Codes for generating a raw table for verification and testing
    #
    # #print $u2b{"F7D1"}, "\n";
    # print $b2u{$u2b{"F7D1"}}, "\n";
    # print "FA59 -> U+", $b2u{"FA59"}, "\n";

    foreach $big5 ( sort { hex($a) <=> hex($b) } keys %b2u ) {
        $unicode = $b2u{$big5};
        $big5 =~ s/^00//;
        print "U+", $unicode, ": ", $big5, "\n";
    }
}

sub gen_check_u2b() {
    foreach $unicode ( sort { hex($a) <=> hex($b) } keys %u2b ) {
        $big5 = $u2b{$unicode};
        $big5 =~ s/^00//;
        print "U+", $unicode, ": ", $big5, "\n";
    }

}

###########################################################################
#
#  Codes for generating hkscs.ut and hkscs.uf files for Mozilla
#
sub gen_mozilla_uf() {
    # hkscs.uf
    foreach $unicode ( sort keys %u2b ) {
        $big5 = $u2b{$unicode};
	my $b = hex($big5);
        print "0x", uc($big5), "\t0x", uc($unicode), "\n"
          unless ( $b >= 0xA140 and $b <= 0xC6A0 )
          or ( $b >= 0xC940 and $b <= 0xF9D5 )
          or ( $b < 0x8140 )
          or ( hex($unicode) > 0xFFFF );
    }
}

sub gen_mozilla_ut() {
    # hkscs.ut
    foreach $big5 ( sort keys %b2u ) {
        my $b = hex($big5);
        print "0x", uc($big5), "\t0x", uc( $b2u{$big5} ), "\n"
          unless ( $b >= 0xA140 and $b <= 0xC6A0 )
	  or ( $b < 0x8140 )
          or ( $b >= 0xC940 and $b <= 0xF9D5 );
    }
}


###########################################################################

sub gen_glibc() {

    ##########################################################################
    #
    #   Generate index for UCS4 to Big5-HKSCS conversion table
    #
    @index_array = ();

    $mode  = 0;
    $count = 0;
    for ( $uni = 0x81 ; $uni <= 0x2FFFF ; $uni++ ) {
        $unicode = sprintf( "%04X", $uni );

        # print "  /* U+$unicode */\t" if $low % 4 == 0;
        if ( defined( $u2b{$unicode} ) ) {
            if ( $mode == 0 ) {
                $range_start = $range_end = $uni;

                # printf "  { %7s, ", sprintf("0x%04X", $range_start);
                $mode = 1;
            }
            else {
                $range_end = $uni;
            }
        }
        elsif ( $mode == 1 and ( $uni - $range_end ) >= 0x80 ) {

            # Start a new range if the gap is 0x80 or larger
            # printf "%7s, %5d },\n", sprintf("0x%04X", $range_end), $count;
            push @index_array, [ ( $range_start, $range_end, $count ) ];
            $count += $range_end - $range_start + 1;
            $mode = 0;
        }
    }

    #
    #  Note that $count and $range_end are used again as global variables
    #  below
    #

    ###########################################################################
    #
    #  Start generating real C code...
    #

    print <<'EOT';
/* Mapping tables for Big5-HKSCS handling.
   Copyright (C) 1997, 1998, 2000, 2001, 2002 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Ulrich Drepper <drepper@cygnus.com>, 1997.
   Modified for Big5-HKSCS by Roger So <roger.so@sw-linux.com>, 2000.
   Updated for HKSCS-2001 by James Su <suzhe@turbolinux.com.cn>
                         and Anthony Fok <anthony@thizlinux.com>, 2002

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <dlfcn.h>
#include <gconv.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>


/* Table for Big5-HKSCS to UCS conversion.

   Original comments by Roger So when he updated the tables for HKSCS-1999:

     With HKSCS mappings 0x8140-0xA0FE and 0xFA40-0xFEFE added; more info:
     http://www.digital21.gov.hk/eng/hkscs/index.html
       - spacehunt 07/01/2000

   The BIG5-HKSCS mapping tables are generated from 950.txt, big5-iso.txt
   and big5cmp.txt using a Perl script while merging C source code from
   other developers.  A copy of the source Perl script is available at:

      http://www.thizlinux.com/~anthony/hkscs/gen-glibc-big5hkscs.pl
      http://people.debian.org/~foka/hkscs/gen-glibc-big5hkscs.pl

  Revisions:
    2001-10-30  made codec for Qt
    2002-03-21  ported to glibc-2.2.5 and added HKSCS-2001

  Todo:
    Use a hash for characters beyond BMP to save space and make it
    more efficient

   - Anthony Fok <anthony@thizlinux.com>  21 Mar 2002
     On behalf of ThizLinux Laboratory Ltd., Hong Kong SAR, China
*/

EOT

    ##########################################################################
    #
    # Generate Big5-HKSCS to Unicode conversion table
    #

    ## print "Big5HKSCS to Unicode\n";

    # for $high (0x81..0x8d, 0x8e..0xa0, 0xc6..0xc8, 0xf9, 0xfa..0xfe) {

    $high_start = 0x88;
    $high_end   = 0xfe;

    print "static const uint16_t big5_hkscs_to_ucs[";
    print( ( $high_end - $high_start + 1 ) * 157 );
    print "] =\n{\n";
    for $high ( 0x88 .. 0xfe ) {
        for $low ( 0x40 .. 0x7e, 0xa1 .. 0xfe ) {
            if ( $low == 0x40 ) {
                print "\n" unless $high == $high_start;
                printf
                  "\t/* Big5-HKSCS 0x%02X40..0x%02X7E, 0x%02XA1..0x%02XFE */\n",
                  $high, $high, $high, $high;
            }
            elsif ( $low == 0xa1 ) {
                print "\t\t";
            }
            $big5 = sprintf( "%02X%02X", $high, $low );
            print "\t" if $low % 8 == 0;
            if ( defined( $b2u{$big5} ) ) {
                $unicode = $b2u{$big5};
                print "0x", $unicode, ",";
            }
            else {
                print "0x0000,";    # for glibc
            }
            print( ( $low % 8 == 7 or $low == 0x7e or $low == 0xfe ) 
                ? "\n"
                : "\t" );
        }
    }
    print "};\n\n";

    ##########################################################################
    #
    # Generate Unicode to Big5-HKSCS conversion table
    #
    print "static const unsigned char ucs4_to_big5_hkscs[$count][2] =\n{\n";
    foreach $index (@index_array) {
        ( $start, $end ) = ( @$index[0], @$index[1] );
        printf( "  /* U+%04X */\t", $start ) if ( $start % 4 != 0 );
        print "\t" x ( ( $start % 4 ) * 1.5 ) . "    " x ( $start % 2 );
        for ( $i = $start ; $i <= $end ; $i++ ) {
            printf( "  /* U+%04X */\t", $i ) if ( $i % 4 == 0 );
            $unicode = sprintf( "%04X", $i );
            if ( defined( $big5 = $u2b{$unicode} ) ) {
                if ( $big5 =~ /^00/ ) {
                    print '"\x', substr( $big5, 2, 2 ), '\x00",';
                }
                else {
                    print '"\x', substr( $big5, 0, 2 ), '\x',
                      substr( $big5, 2, 2 ), '",';
                }
            }
            else {
                print '"\x00\x00",';
            }
            print( ( $i % 4 == 3 ) ? "\n" : " " ) unless $i == $end;
        }
        print $end == $range_end ? "\n" : "\n\n";
    }
    print "};\n\n";

    ###########################################################################

    print <<EOT;
static struct
{
    /* Note: We are going to split this table so that we can use
       uint16_t for "from" and "to" again.  Anthony Fok, 2002-03-21 */
    uint32_t from;
    uint32_t to;
    uint32_t offset;
} from_ucs4_idx[] =
{
EOT
    foreach $index (@index_array) {
        printf "    { %7s, %7s, %5d },\n", sprintf( "0x%04X", @$index[0] ),
          sprintf( "0x%04X", @$index[1] ), @$index[2];
    }
    print "};\n\n";

    #foreach $i (sort keys %b2u) {
    #    print $b2u{$i} . ' ';
    #}

    print <<'EOT';
/* Definitions used in the body of the `gconv' function.  */
#define CHARSET_NAME		"BIG5HKSCS//"
#define FROM_LOOP		from_big5
#define TO_LOOP			to_big5
#define DEFINE_INIT		1
#define DEFINE_FINI		1
#define MIN_NEEDED_FROM		1
#define MAX_NEEDED_FROM		2
#define MIN_NEEDED_TO		4


/* First define the conversion function from Big5-HKSCS to UCS4.  */
#define MIN_NEEDED_INPUT	MIN_NEEDED_FROM
#define MAX_NEEDED_INPUT	MAX_NEEDED_FROM
#define MIN_NEEDED_OUTPUT	MIN_NEEDED_TO
#define LOOPFCT			FROM_LOOP
#define BODY \
  {									      \
    uint32_t ch = *inptr;						      \
									      \
    if (ch >= 0x81 && ch <= 0xfe)					      \
      {									      \
	/* Two-byte character.  First test whether the next character	      \
	   is also available.  */					      \
	uint32_t ch2;							      \
	int idx;							      \
									      \
	if (__builtin_expect (inptr + 1 >= inend, 0))			      \
	  {								      \
	    /* The second character is not available.  */		      \
	    result = __GCONV_INCOMPLETE_INPUT;				      \
	    break;							      \
	  }								      \
									      \
	ch2 = inptr[1];							      \
	/* See whether the second byte is in the correct range.  */	      \
	if ((ch2 >= 0x40 && ch2 <= 0x7e) || (ch2 >= 0xa1 && ch2 <= 0xfe))     \
	  {								      \
	    if (ch >= 0x88)						      \
	      {								      \
		/* Look up the table */					      \
		idx = (ch - 0x88) * 157 + ch2 - (ch2 <= 0x7e ? 0x40 : 0x62);  \
		if ((ch = big5_hkscs_to_ucs[idx]) == 0)			      \
		  {							      \
		    /* This is illegal.  */				      \
		    if (! ignore_errors_p ())				      \
		      {							      \
			result = __GCONV_ILLEGAL_INPUT;			      \
			break;						      \
		      }							      \
									      \
		    ++inptr;						      \
		    ++*irreversible;					      \
		    continue;						      \
		  }							      \
	      }								      \
	    else							      \
	      {								      \
		/* 0x81..0x87 in UDA3, currently maps linearly to PUA */      \
		ch = (ch - 0x81) * 157 + ch2 - (ch2 <= 0x7e ? 0x40 : 0x62)    \
		      + 0xeeb8;						      \
	      }								      \
	  }								      \
	else								      \
	  {								      \
	    /* This is illegal.  */					      \
	    if (! ignore_errors_p ())					      \
	      {								      \
		result = __GCONV_ILLEGAL_INPUT;				      \
		break;							      \
	      }								      \
									      \
	    ++inptr;							      \
	    ++*irreversible;						      \
	    continue;							      \
	  }								      \
									      \
	inptr += 2;							      \
      }									      \
    else if (__builtin_expect (ch, 0) == 0xff)				      \
      {									      \
	result = __GCONV_ILLEGAL_INPUT;					      \
	break;								      \
      }									      \
    else  /* 0x00 to 0x80 */						      \
      ++inptr;								      \
									      \
    put32 (outptr, ch);							      \
    outptr += 4;							      \
  }
#define LOOP_NEED_FLAGS
#include <iconv/loop.c>


/* Next, define the other direction.  */
#define MIN_NEEDED_INPUT	MIN_NEEDED_TO
#define MIN_NEEDED_OUTPUT	MIN_NEEDED_FROM
#define MAX_NEEDED_OUTPUT	MAX_NEEDED_FROM
#define LOOPFCT			TO_LOOP
#define BODY \
  {									      \
    uint32_t ch = get32 (inptr);					      \
    const unsigned char *cp = "";						      \
    unsigned char b5ch[2] = "\0\0";					      \
    int i;								      \
    									      \
    for (i = 0;								      \
	 i < (int) (sizeof (from_ucs4_idx) / sizeof (from_ucs4_idx[0]));      \
	 ++i)								      \
      {									      \
	if (ch < from_ucs4_idx[i].from)					      \
	  break;							      \
	if (from_ucs4_idx[i].to >= ch)					      \
	  {								      \
	    cp = ucs4_to_big5_hkscs[from_ucs4_idx[i].offset		      \
			  + ch - from_ucs4_idx[i].from];		      \
	    break;							      \
	  }								      \
      }									      \
									      \
    if (ch <= 0x80)							      \
      {									      \
	b5ch[0] = ch;							      \
	cp = b5ch;							      \
      }									      \
									      \
    if (cp[0] == '\0' && ch != 0)					      \
      {									      \
	UNICODE_TAG_HANDLER (ch, 4);					      \
									      \
	/* Illegal character.  */					      \
	STANDARD_ERR_HANDLER (4);					      \
      }									      \
    else								      \
      {									      \
	/* See whether there is enough room for the second byte we write.  */ \
	if (__builtin_expect (cp[1], '\1') != '\0'			      \
	    && __builtin_expect (outptr + 1 >= outend, 0))		      \
	  {								      \
	    /* We have not enough room.  */				      \
	    result = __GCONV_FULL_OUTPUT;				      \
	    break;							      \
	  }								      \
									      \
	*outptr++ = cp[0];						      \
	if (cp[1] != '\0')						      \
	  *outptr++ = cp[1];						      \
      }									      \
									      \
    inptr += 4;								      \
  }
#define LOOP_NEED_FLAGS
#include <iconv/loop.c>


/* Now define the toplevel functions.  */
#include <iconv/skeleton.c>
EOT

}
