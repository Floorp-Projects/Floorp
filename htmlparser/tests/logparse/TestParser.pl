#! /usr/bin/perl

 use Cwd;

 $path=`pwd`;;

 $path=~m/mozilla/;

 $drive=$`;

 die "\nUsage: perl TestParser.pl [-b|-v] <filelist>
 b -> create baseline 
 v -> verify changes\n"

 if(@ARGV < 2 || @ARGV > 2);

 open(FILE_LIST,$ARGV[1]) || die "\nCannot open $ARGV[1]\n";

 if($ARGV[0] eq "-b") {
   foreach $input(<FILE_LIST>) {
     $input =~s/\n//g;
     @output=split(/\./,$input);
     print "\n$input\n";
     system("$drive\mozilla/dist/WIN32_D.obj/bin/TestParser.exe $input $output[0].b");
   }
 }
 elsif($ARGV[0] eq "-v") {
   foreach $input(<FILE_LIST>) {
     $input =~s/\n//g;
     @output=split(/\./,$input);
     print "\n$input\n";
     system("$drive\mozilla/dist/WIN32_D.obj/bin/TestParser.exe $input $output[0].v");
     system("fc $output[0].b $output[0].v");
   }
 }
 else {
   print "\n\"$ARGV[0]\" unknown....\n";
   print "\nUsage: perl TestParser.pl [-b|-v] <filelist>\n\n";
 }
 
 close(FILE_LIST);
