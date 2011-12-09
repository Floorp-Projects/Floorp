
Test fonts with Graphite tables for testing

Original font: League Gothic by League of Moveable Type, converted to TT via FontSquirrel

Kerning, GDEF/GSUB/GPOS info stripped out and the name table edited to
produce grtest-template.ttx.

Making a test font:

1. Build ttx from template
  sed -e 's/xxxfontnamexxx/grtestxxx/' grtest-template.ttx >grtestxxx-src.ttx

2. Make the font
  ttx grtestxxx-src.ttx
  
3. Edit GDL file

4. Compile the GDL into the ttf

../graphite-compiler.sh -d -v3 -w3521 -w510 font.gdl grtestxxx-src.ttf grtestxxx.ttf

Where graphite-compiler.sh is a script to run the graphite compiler in wine on OSX
(the compiler is available both as a Windows exe and as source).

Graphite compiler download:
http://scripts.sil.org/cms/scripts/page.php?item_id=GraphiteCompilerDownload

grtest-simple.ttf
Simple FAIL ==> PaSs substitution via graphite or via OT with TST1=1

grtest-multipass.ttf
Several passes where the end result is FAIL ==> PaSs or via OT with TST1=1

grtest-langfeat.ttf
FAIL ==> PaSs substitution enabled via language or feature settings or via OT with TST1=1

