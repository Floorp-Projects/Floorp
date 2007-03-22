#
# $Id: UCDATAREADME.txt,v 1.1 1999/01/08 00:19:20 ftang%netscape.com Exp $
#

                           MUTT UCData Package 1.9
                           -----------------------

This is a package that supports ctype-like operations for Unicode UCS-2 text
(and surrogates), case mapping, and decomposition lookup.  To use it, you will
need to get the "UnicodeData-2.0.14.txt" (or later) file from the Unicode Web
or FTP site.

This package consists of two parts:

  1. A program called "ucgendat" which generates five data files from the
     UnicodeData-2.*.txt file.  The files are:

     A. case.dat   - the case mappings.
     B. ctype.dat  - the character property tables.
     C. decomp.dat - the character decompositions.
     D. cmbcl.dat  - the non-zero combining classes.
     E. num.dat    - the codes representing numbers.

  2. The "ucdata.[ch]" files which implement the functions needed to
     check to see if a character matches groups of properties, to map between
     upper, lower, and title case, to look up the decomposition of a
     character, look up the combining class of a character, and get the number
     value of a character.

A short reference to the functions available is in the "api.txt" file.

Techie Details
==============

The "ucgendat" program parses files from the command line which are all in the
Unicode Character Database (UCDB) format.  An additional properties file,
"MUTTUCData.txt", provides some extra properties for some characters.

The program looks for the two character properties fields (2 and 4), the
combining class field (3), the decomposition field (5), the numeric value
field (8), and the case mapping fields (12, 13, and 14).  The decompositions
are recursively expanded before being written out.

The decomposition table contains all the canonical decompositions.  This means
all decompositions that do not have tags such as "<compat>" or "<font>".

The data is almost all stored as unsigned longs (32-bits assumed) and the
routines that load the data take care of endian swaps when necessary.  This
also means that surrogates (>= 0x10000) can be placed in the data files the
"ucgendat" program parses.

The data is written as external files and broken into five parts so it can be
selectively updated at runtime if necessary.

The data files currently generated from the "ucgendat" program total about 56K
in size all together.

The format of the binary data files is documented in the "format.txt" file.

Mark Leisher <mleisher@crl.nmsu.edu>
13 December 1998

CHANGES
=======

Version 1.9
-----------
1. Fixed a problem with an incorrect amount of storage being allocated for the
   combining class nodes.

2. Fixed an invalid initialization in the number code.

3. Changed the Java template file formatting a bit.

4. Added tables and function for getting decompositions in the Java class.

Version 1.8
-----------
1. Fixed a problem with adding certain ranges.

2. Added two more macros for testing for identifiers.

3. Tested with the UnicodeData-2.1.5.txt file.

Version 1.7
-----------
1. Fixed a problem with looking up decompositions in "ucgendat."

Version 1.6
-----------
1. Added two new properties introduced with UnicodeData-2.1.4.txt.

2. Changed the "ucgendat.c" program a little to automatically align the
   property data on a 4-byte boundary when new properties are added.

3. Changed the "ucgendat.c" programs to only generate canonical
   decompositions.

4. Added two new macros ucisinitialpunct() and ucisfinalpunct() to check for
   initial and final punctuation characters.

5. Minor additions and changes to the documentation.

Version 1.5
-----------
1. Changed all file open calls to include binary mode with "b" for DOS/WIN
   platforms.

2. Wrapped the unistd.h include so it won't be included when compiled under
   Win32.

3. Fixed a bad range check for hex digits in ucgendat.c.

4. Fixed a bad endian swap for combining classes.

5. Added code to make a number table and associated lookup functions.
   Functions added are ucnumber(), ucdigit(), and ucgetnumber().  The last
   function is to maintain compatibility with John Cowan's "uctype" package.

Version 1.4
-----------
1. Fixed a bug with adding a range.

2. Fixed a bug with inserting a range in order.

3. Fixed incorrectly specified ucisdefined() and ucisundefined() macros.

4. Added the missing unload for the combining class data.

5. Fixed a bad macro placement in ucisweak().

Version 1.3
-----------
1. Bug with case mapping calculations fixed.

2. Bug with empty character property entries fixed.

3. Bug with incorrect type in the combining class lookup fixed.

4. Some corrections done to api.txt.

5. Bug in certain character property lookups fixed.

6. Added a character property table that records the defined characters.

7. Replaced ucisunknown() with ucisdefined() and ucisundefined().

Version 1.2
-----------
1. Added code to ucgendat to generate a combining class table.

2. Fixed an endian problem with the byte count of decompositions.

3. Fixed some minor problems in the "format.txt" file.

4. Removed some bogus "Ss" values from MUTTUCData.txt file.

5. Added API function to get combining class.

6. Changed the open mode to "rb" so binary data files will be opened correctly
   on DOS/WIN as well as other platforms.

7. Added the "api.txt" file.

Version 1.1
-----------
1. Added ucisxdigit() which I overlooked.

2. Added UC_LT to the ucisalpha() macro which I overlooked.

3. Change uciscntrl() to include UC_CF.

4. Added ucisocntrl() and ucfntcntrl() macros.

5. Added a ucisblank() which I overlooked.

6. Added missing properties to ucissymbol() and ucisnumber().

7. Added ucisgraph() and ucisprint().

8. Changed the "Mr" property to "Sy" to mark this subset of mirroring
   characters as symmetric to avoid trampling the Unicode/ISO10646 sense of
   mirroring.

9. Added another property called "Ss" which includes control characters
   traditionally seen as spaces in the isspace() macro.

10. Added a bunch of macros to be API compatible with John Cowan's package.

ACKNOWLEDGEMENTS
================

Thanks go to John Cowan <cowan@locke.ccil.org> for pointing out lots of
missing things and giving me stuff, particularly a bunch of new macros.

Thanks go to Bob Verbrugge <bob_verbrugge@nl.compuware.com> for pointing out
various bugs.

Thanks go to Christophe Pierret <cpierret@businessobjects.com> for pointing
out that file modes need to have "b" for DOS/WIN machines, pointing out
unistd.h is not a Win 32 header, and pointing out a problem with ucisalnum().

Thanks go to Kent Johnson <kent@pondview.mv.com> for finding a bug that caused
incomplete decompositions to be generated by the "ucgendat" program.

Thanks go to Valeriy E. Ushakov <uwe@ptc.spbu.ru> for spotting an allocation
error and an initialization error.
