#!/perl

# make-jars [-d <destPath>] < <manifest.jr>

sub JarIt
{
    my ($jarfile, $args) = @_;
    print "+++ jaring $jarfile\n";
    flush;
    system "zip -u $jarfile $args\n";
}

sub MkDirs
{
    my ($path) = @_;
    if ($path =~ /([\w\d.\-]+)[\\\/](.*)/) {
        my $dir = $1;
        $path = $2;
        if (!-e $dir) {
            mkdir($dir, 0777) || die "error: can't create '$dir': $!";
        }
        chdir $dir;
        MkDirs($path);
        chdir "..";
    }
    else {
        my $dir = $path;
        if (!-e $dir) {
            mkdir($dir, 0777) || die "error: can't create '$dir': $!";
        }
    }
}

sub CopyFile
{
    my ($from, $to) = @_;
    open(OUT, ">$to") || die "error: can't open '$to': $!";
    open(IN, "<$from") || die "error: can't open '$from': $!";
    while (<IN>) {
        print OUT $_;
    }
    close(IN) || die "error: can't close '$from': $!";
    close(OUT) || die "error: can't close '$to': $!";
}

sub EnsureFileInDir
{
    my ($path) = @_;
    if (!-e $path) {
        $path =~ /(.*)[\\\/]([\w\d.\-]+)/;
        my $dir = $1;
        my $file = $2;
        if (!-e $file) {
            die "error: file '$file' doesn't exist";
        }
        MkDirs($dir);
        CopyFile($file, $path);
        return 1;
    }
    return 0;
}

use Getopt::Std;

getopt("d:");

my $destPath = ".";
if (defined($opt_d)) {
    $destPath = $opt_d;
}

while (<>) {
    chomp;
  start: 
    if (/^([\w\d.\-\\\/]+)\:\s*$/) {
        my $jarfile = "$destPath/$1";
        my $args = "";
        while (<>) {
            if (/^\s+([\w\d.\-\\\/]+)\s*$/) {
                my $arg = $1;
                my $removeDir = EnsureFileInDir($arg);
                $args = "$args$arg ";
            } elsif (/^\s*$/) {
                # end with blank line
                last;
            } else {
                JarIt($jarfile, $args);
                goto start;
            }
        }
        JarIt($jarfile, $args);

    } elsif (/^\s*\#.*$/) {
        # skip comments
    } elsif (/^\s*$/) {
        # skip blank lines
    } else {
        close;
        die "bad jar rule head at: $_";
    }
}
