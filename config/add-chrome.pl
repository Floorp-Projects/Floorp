#!/perl

use File::Path;

my $installedChromeFile = $ARGV[0];
my $disableJarPackaging = $ARGV[1];
my $chromeType = $ARGV[2];
my $pkgName = $ARGV[3];
my $jarFileName = $ARGV[4];

#print "add-chrome $installedChromeFile $disableJarPackaging $chromeType $pkgName $jarFileName\n";

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

if (open(FILE, "<$installedChromeFile")) {
    while (<FILE>) {
        chomp;
        if ($_ =~ $line) {
            # line already appears in installed-chrome.txt file
            # just update the mod date
            close(FILE) || die "error: can't close $installedChromeFile: $!";
            my $now = time;
            utime($now, $now, $installedChromeFile) || die "couldn't touch $installedChromeFile";
            print "+++ updating chrome $installedChromeFile\n+++\t$line\n";
            exit;
        }
    }
    close(FILE) || die "error: can't close $installedChromeFile: $!";
}

my $dir = $installedChromeFile;
if ("$dir" =~ /([\w\d.\-\\\/]+)[\\\/]([\w\d.\-]+)/) {
    $dir = $1;
}
mkpath($dir, 0, 0755);

open(FILE, ">>$installedChromeFile") || die "can't open $installedChromeFile: $!";
print FILE "$line\n";
close(FILE) || die "error: can't close $installedChromeFile: $!";
print "+++ adding chrome $installedChromeFile\n+++\t$line\n";
