#!/perl

# make-jars [-d <destPath>] < <jar.mn>

use Getopt::Std;
use Cwd;
use File::stat;
use Time::localtime;

@cleanupList = ();
$IS_DIR = 1;
$IS_FILE = 2;

getopt("d:");
my $destPath = ".";
if (defined($opt_d)) {
    $destPath = $opt_d;
}

getopt("c:");
my $copyFiles = false;
if (defined($opt_c)) {
    $copyFiles = true;
}

getopt("v:");
my $verbose = false;
if (defined($opt_v)) {
    $verbose = true;
}

sub Cleanup
{
    while (true) {
        my $isDir = pop(@cleanupList);
        if ($isDir == undef) {
            return 0;
        }
        my $path = pop(@cleanupList);
        if ($isDir == $IS_DIR) {
            rmdir($path) || die "can't remove dir $path: $!";
        }
        else {
            unlink($path) || die "can't remove file $path: $!";
        }
    }
}

sub JarIt
{
    my ($destPath, $jarfile, $copyFiles, $args, $overrides) = @_;

    my $dir = $destPath;
    if ("$dir/$jarfile" =~ /([\w\d.\-\\\/]+)[\\\/]([\w\d.\-]+)/) {
        $dir = $1;
    }
    MkDirs($dir, ".", false);

    if ($copyFiles eq true) {
        my $indivDir = $jarfile;
        if ($indivDir =~ /(.*).jar/) {
            $indivDir = $1;
        }
        my $indivPath = "$destPath/$indivDir";
        MkDirs($indivPath, ".", false);
        
        foreach $file (split(' ', $args)) {
            if ($verbose eq true) {
                print "adding individual file $file to dist\n";
            }
            EnsureFileInDir("$indivPath/$file", $file, false, false);
        }
        foreach $file (split(' ', $overrides)) {
            if ($verbose eq true) {
                print "adding individual file $file to dist\n";
            }
            EnsureFileInDir("$indivPath/$file", $file, false, true);
        }
    }

    if (!($args eq "")) {
        system "zip -u $destPath/$jarfile $args\n" || die "zip failed";
    }
    if (!($overrides eq "")) {
        print "+++ overriding $overrides\n";
        system "zip $destPath/$jarfile $overrides\n" || die "zip failed";
    }

    my $cwd = cwd();
    print "+++ made chrome $cwd => $destPath/$jarfile\n";
    Cleanup();
}

sub MkDirs
{
    my ($path, $containingDir, $doCleanup) = @_;
    #print "MkDirs $path $containingDir $doCleanup\n";
    if ($path =~ /([\w\d.\-]+)[\\\/](.*)/) {
        my $dir = $1;
        $path = $2;
        if (!-e $dir) {
            if ($verbose eq true) {
                print "making dir $containingDir/$dir\n";
            }
            mkdir($dir, 0777) || die "error: can't create '$dir': $!";
            if ($doCleanup eq true) {
                push(@cleanupList, "$containingDir/$dir");
                push(@cleanupList, $IS_DIR);
            }
        }
        my $wd = cwd();
        chdir $dir;
        MkDirs($path, "$containingDir/$dir", $doCleanup);
        chdir $wd;
    }
    else {
        my $dir = $path;
        if ($dir eq "") { return 0; } 
        if (!-e $dir) {
            if ($verbose eq true) {
                print "making dir $containingDir/$dir\n";
            }
            mkdir($dir, 0777) || die "error: can't create '$dir': $!";
            if ($doCleanup eq true) {
                push(@cleanupList, "$containingDir/$dir");
                push(@cleanupList, $IS_DIR);
            }
        }
    }
}

sub CopyFile
{
    my ($from, $to, $doCleanup) = @_;
    if ($verbose eq true) {
        print "copying $from to $to\n";
    }
    open(OUT, ">$to") || die "error: can't open '$to': $!";
    open(IN, "<$from") || die "error: can't open '$from': $!";
    binmode IN;
    binmode OUT;
    my $len;
    my $buf;
    while ($len = sysread(IN, $buf, 4096)) {
        if (!defined $len) {
            next if $! =~ /^Interrupted/;
            die "System read error: $!\n";
        }
        my $offset = 0;
        while ($len) {
            my $written = syswrite(OUT, $buf, $len, $offset);
            die "System write error: $!\n" unless defined $written;
            $len -= $written;
            $offset += $written;
        }
    }
    close(IN) || die "error: can't close '$from': $!";
    close(OUT) || die "error: can't close '$to': $!";

    # fix the mod date so we don't jar everything (is this faster than just jarring everything?)
    my $atime = stat($from)->atime || die $!;
    my $mtime = stat($from)->mtime || die $!;
    utime($atime, $mtime, $to);

    if ($doCleanup eq true) {
        push(@cleanupList, "$to");
        push(@cleanupList, $IS_FILE);
    }
}

sub EnsureFileInDir
{
    my ($destPath, $srcPath, $doCleanup, $override) = @_;

    if (!-e $destPath || $override eq true) {
        my $dir = "";
        my $file;
        if ($destPath =~ /([\w\d.\-\\\/]+)[\\\/]([\w\d.\-]+)/) {
            $dir = $1;
            $file = $2;
        }
        else {
            $file = $destPath;
        }

        if ($srcPath) {
            $file = $srcPath;
        }

        if (!-e $file) {
            die "error: file '$file' doesn't exist\n";
        }
        MkDirs($dir, ".", $doCleanup);
        CopyFile($file, $destPath, $doCleanup);
        return 1;
    }
#    elsif ($doCleanup eq false && -e $destPath) {
#        print "!!! file $destPath already exists -- need to add '+' rule to jar.mn\n";
#    }
    return 0;
}

while (<>) {
    chomp;
  start: 
    if (/^([\w\d.\-\\\/]+)\:\s*$/) {
        my $jarfile = $1;
        my $args = "";
        my $overrides = "";
        while (<>) {
            if (/^\s+([\w\d.\-\\\/]+)\s*(\([\w\d.\-\\\/]+\))?$\s*/) {
                my $dest = $1;
                my $srcPath = $2;

                if ( $srcPath ) {  
                    $srcPath = substr($srcPath,1,-1);
                }

                EnsureFileInDir($dest, $srcPath, true);
                $args = "$args$dest ";
            } elsif (/^\+\s+([\w\d.\-\\\/]+)\s*(\([\w\d.\-\\\/]+\))?$\s*/) {
                my $dest = $1;
                my $srcPath = $2;

                if ( $srcPath ) {  
                    $srcPath = substr($srcPath,1,-1);
                }

                EnsureFileInDir($dest, $srcPath, true);
                $overrides = "$overrides$dest ";
            } elsif (/^\s*$/) {
                # end with blank line
                last;
            } else {
                JarIt($destPath, $jarfile, $copyFiles, $args, $overrides);
                goto start;
            }
        }
        JarIt($destPath, $jarfile, $copyFiles, $args, $overrides);

    } elsif (/^\s*\#.*$/) {
        # skip comments
    } elsif (/^\s*$/) {
        # skip blank lines
    } else {
        close;
        die "bad jar rule head at: $_";
    }
}
