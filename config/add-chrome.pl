#!/perl

# add-chrome <jar-file-name> <pkg-name> <chrome-type> <installed-chrome.txt-file>

my $installedChromeFile = $ARGV[0];
my $chromeType = $ARGV[1];
my $pkgName = $ARGV[2];
my $jarFileName = $ARGV[3];
my $disableJarPackaging = $ARGV[4];

#print "add-chrome $jarFileName $pkgName $chromeType $installedChromeFile\n";

my $line;
if ($disableJarPackaging) {
    $line = "$chromeType,install,url,resource:/chrome/$jarFileName/";
}
else {
    $line = "$chromeType,install,url,jar:resource:/chrome/$jarFileName.jar!/";
}
#coming...
#my $line = "$chromeType,install,url,jar:resource:/chrome/$jarFileName!/$chromeType/$pkgName/"; 

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
