#!/perl
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


use File::Path;
use Getopt::Std;
use IO::File;
use mozLock;

getopts("lxo:");

my $installedChromeFile = $ARGV[0];
my $disableJarPackaging = $ARGV[1];
my $chromeType = $ARGV[2];
my $pkgName = $ARGV[3];
my $jarFileName = $ARGV[4];

my $win32 = ($^O =~ /((MS)?win32)|msys|cygwin|os2/i) ? 1 : 0;
my $macos = ($^O =~ /MacOS|darwin/i) ? 1 : 0;
my $unix  = !($win32 || $macos) ? 1 : 0;

sub foreignPlatformFile
{
   my ($jarfile) = @_;
   
   if (!$win32 && index($jarfile, "-win") != -1) {
     return 1;
   }
   
   if (!$unix && index($jarfile, "-unix") != -1) {
     return 1; 
   }

   if (!$macos && index($jarfile, "-mac") != -1) {
     return 1;
   }

   return 0;
}

sub foreignPlatformPath
{
   my ($jarpath) = @_;
   
   if (!$win32 && index($jarpath, "-platform/win") != -1) {
     return 1;
   }
   
   if (!$unix && index($jarpath, "-platform/unix") != -1) {
     return 1; 
   }

   if (!$macos && index($jarpath, "-platform/mac") != -1) {
     return 1;
   }

   return 0;
}

#print "add-chrome $installedChromeFile $disableJarPackaging $chromeType $pkgName $jarFileName\n";

my $nofilelocks = 0;
if (defined($::opt_l)) {
    $nofilelocks = 1;
}

if (defined($::opt_x)) {
    $win32 = 0;
    $macos = 0;
    $unix = 1;
}

my $force_os;
if (defined($::opt_o)) {
    $force_os = $::opt_o;
}

if (defined($force_os)) {
    $win32 = 0;
    $macos = 0;
    $unix = 0;
    if ($force_os eq "WINNT") {
	$win32 = 1;
    } elsif ($force_os eq "OS2") {
	$win32 = 1;
    } elsif ($force_os eq "Darwin") {
	$macos = 1;
    } else {
	$unix = 1;
    }
}

if ($jarFileName =~ /(.*)\.jar/) {
    $jarFileName = $1;
}

if (!foreignPlatformFile($jarFileName) && !foreignPlatformPath($pkgName)) {

my $line;
if ($disableJarPackaging) {
    $line = "$chromeType,install,url,resource:/chrome/$jarFileName/$chromeType/$pkgName/";
}
else {
    $line = "$chromeType,install,url,jar:resource:/chrome/$jarFileName.jar!/$chromeType/$pkgName/";
}

my $lockfile = "$installedChromeFile.lck";
my $err;

mozLock($lockfile) if (!$nofilelocks);
$err = 0;
if (open(FILE, "<$installedChromeFile")) {
    while (<FILE>) {
        chomp;
        if ($_ eq $line) {
            # line already appears in installed-chrome.txt file
            # just update the mod date
            close(FILE) or $err = 1; 
	    if ($err) {
		mozUnlock($lockfile) if (!$nofilelocks);
		die "error: can't close $installedChromeFile: $!";
	    }
            my $now = time;
            utime($now, $now, $installedChromeFile) or $err = 1;
	    mozUnlock($lockfile) if (!$nofilelocks);
	    if ($err) {
		die "couldn't touch $installedChromeFile";
	    }
            print "+++ updating chrome $installedChromeFile\n+++\t$line\n";
            exit;
        }
    }
    close(FILE) or $err = 1;
    if ($err) {
	mozUnlock($lockfile) if (!$nofilelocks);
	die "error: can't close $installedChromeFile: $!";
    }
}
mozUnlock($lockfile) if (!$nofilelocks);

my $dir = $installedChromeFile;
if ("$dir" =~ /([\w\d.\-\\\/]+)[\\\/]([\w\d.\-]+)/) {
    $dir = $1;
}
mkpath($dir, 0, 0755);

mozLock($lockfile) if (!$nofilelocks);
$err = 0;
open(FILE, ">>$installedChromeFile") or $err = 1;
if ($err) {
    mozUnlock($lockfile) if (!$nofilelocks);
    die "can't open $installedChromeFile: $!";
}
print FILE "$line\n";
close(FILE) or $err = 1;
mozUnlock($lockfile) if (!$nofilelocks);
if ($err) {
    die "error: can't close $installedChromeFile: $!";
}
print "+++ adding chrome $installedChromeFile\n+++\t$line\n";
}

