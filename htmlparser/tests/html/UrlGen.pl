#!/usr/bin/perl

use Cwd;

$curr_dir=`cd`;

open(OUTFILE,">url_list.txt") || die "Can't open url.txt $!";
opendir(D,".");

@files=readdir(D);
$curr_dir=~s/\\/\//g;
chomp($curr_dir);

foreach $file(@files) {
 if($file=~m/\.htm/) {
   print OUTFILE "file:///$curr_dir/$file\n";
 }
}


