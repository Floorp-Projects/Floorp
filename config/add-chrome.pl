#!/perl

use File::Path;
use Getopt::Std;
use IO::File;
use mozLock;

getopts("l");

my $installedChromeFile = $ARGV[0];
my $disableJarPackaging = $ARGV[1];
my $chromeType = $ARGV[2];
my $pkgName = $ARGV[3];
my $jarFileName = $ARGV[4];

#print "add-chrome $installedChromeFile $disableJarPackaging $chromeType $pkgName $jarFileName\n";

my $nofilelocks = 0;
if (defined($::opt_l)) {
    $nofilelocks = 1;
}

if ($jarFileName =~ /(.*)\.jar/) {
    $jarFileName = $1;
}

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
        if ($_ =~ $line) {
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

