#!/perl

use File::Path;
use Fcntl qw(:DEFAULT :flock);
use Getopt::Std;
use IO::File;

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
my $lockhandle = new IO::File;

if (!$nofilelocks) {
    open($lockhandle,">$lockfile") || 
	die("WARNING: Could not create lockfile for $lockfile. Exiting.\n");
    flock($lockhandle, LOCK_EX);
}

if (open(FILE, "<$installedChromeFile")) {
    while (<FILE>) {
        chomp;
        if ($_ =~ $line) {
            # line already appears in installed-chrome.txt file
            # just update the mod date
            close(FILE) || die "error: can't close $installedChromeFile: $!";
	    if (!$nofilelocks) {
		unlink($lockfile);
		flock($lockhandle, LOCK_UN);
	    }
            my $now = time;
            utime($now, $now, $installedChromeFile) || die "couldn't touch $installedChromeFile";
            print "+++ updating chrome $installedChromeFile\n+++\t$line\n";
            exit;
        }
    }
    close(FILE) || die "error: can't close $installedChromeFile: $!";
}
if (!$nofilelocks) {
    unlink($lockfile);
    flock($lockhandle, LOCK_UN);
}

my $dir = $installedChromeFile;
if ("$dir" =~ /([\w\d.\-\\\/]+)[\\\/]([\w\d.\-]+)/) {
    $dir = $1;
}
mkpath($dir, 0, 0755);

if (!$nofilelocks) {
    open($lockhandle,">$lockfile") || 
	die("WARNING: Could not create lockfile for $lockfile. Exiting.\n");
    flock($lockhandle, LOCK_EX);
}
open(FILE, ">>$installedChromeFile") || die "can't open $installedChromeFile: $!";
print FILE "$line\n";
close(FILE) || die "error: can't close $installedChromeFile: $!";
if (!$nofilelocks) {
    unlink($lockfile);
    flock($lockhandle, LOCK_UN);
}
print "+++ adding chrome $installedChromeFile\n+++\t$line\n";

