#!/perl

my $installedChromeFile = $ARGV[0];
my $chromeType = $ARGV[1];
my $pkgName = $ARGV[2];
my $jarFileName = $ARGV[3];
my $disableJarPackaging = $ARGV[4];

#print "add-chrome $installedChromeFile $chromeType $pkgName $jarFileName $disableJarPackaging\n";

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
            my $now = time;
            utime($now, $now, $installedChromeFile) || die "couldn't touch $installedChromeFile";
            print "+++ updating chrome $installedChromeFile\n+++\t$line\n";
            exit;
        }
    }
    close(FILE) || die "error: can't close $installedChromeFile: $!";
}

open(FILE, ">>$installedChromeFile") || die "can't open $installedChromeFile: $!";
print FILE "$line\n";
close(FILE) || die "error: can't close $installedChromeFile: $!";
print "+++ adding chrome $installedChromeFile\n+++\t$line\n";
