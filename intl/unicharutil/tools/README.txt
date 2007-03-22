
* How to generate various properties files in intl/unicharutils/tables and
  header files in intl/unicharutils/src
   ( written by Jungshik Shin for bug 210502 
     https://bugzilla.mozilla.org/show_bug.cgi?id=210502 on 2005-04-05 )

1. Grab the latest version of idnkit at http://www.nic.ad.jp/en/idn/index.html
   (http://www.nic.ad.jp/ja/idn/idnkit/download/index.html )

2. There are three files we need in the kit:
   generate_normalize_data.pl, UCD.pm and SparseMap.pm

3. a. Download the following Unicode data files  :
     CaseFolding.txt,CompositionExclusions.txt, 
     SpecialCasing.txt, UnicodeData.txt

   b. Rename UnicodeData.txt to UnicodeData-Latest.txt

   The latest version is, as of this writing, in 
   ftp://ftp.unicode.org/Public/4.1.0/ucd

4. a. Run generate_normalize_data.pl and save the output to a temporary file
   b. Edit the file 
      - remove the case folding part (search for 'Lowercase' and delete 
        all the lines following it) because we have separate scripts for that, 
      - replace 'unsigned short' and 'unsigned long' with 'PRUnichar' and 
        'PRUint32'
   c. Replace the actual source part (after the license) of 
      intl/unicharutil/src/normalization_data.h with the file you edited.

5. Generate casetable.h and cattable.h with  gencasetable.pl and gencattable.pl
   Just running them will put casetable.h and cattable.h in the right place.
