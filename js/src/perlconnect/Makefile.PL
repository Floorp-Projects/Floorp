use ExtUtils::MakeMaker;

use strict;
use Getopt::Std;

my (%foo, $jsdir, $inc, $libpath);

#m - build under mozilla tree
#d - specifies js build directory (with include/ and lib/ directories)
#c - build under charlie tree
getopts('mcd:', \%foo);

$jsdir = $foo{d};

$foo{'m'} = 1 unless $foo{c} || $foo{d}; #mozilla tree is the default

if ($foo{c}) {
    $inc = "-I$ENV{CHARLIE_HOME}/include";
    $libpath = "-L$ENV{CHARLIE_HOME}/lib";
}

if ($jsdir) {
    $inc = "-I$jsdir/include";
    $libpath = "-L$jsdir/lib -ljs";
}

my $tmpmk = <<'eof';
DEPTH=..
include ../config.mk

all:
	@echo '$(OBJDIR)'
eof

if ($foo{'m'}) {
    if ($^O eq "MSWin32") {
	$inc = "-I.. -I../Debug"; #I'm not sure
	$libpath = "-L../Debug";
    } else { #suppose unix, never Mac, gmake
	open FOO, ">tempmakefile";
	print FOO $tmpmk;
	close FOO;
	my $objdir = `gmake -f tempmakefile`;
	unlink "tempmakefile";
	$inc = "-I.. -I../$objdir";
	$libpath = "-L../$objdir";
    }
}

my %extras = ();
my $define;

if ($^O eq "MSWin32") {
    $define = "-DXP_PC";
    $extras{OBJECT} = '$(BASEEXT)$(OBJ_EXT) jsperl.obj';
} else {
    $define = '-DXP_UNIX';
}

WriteMakefile(NAME => 'JS',
	      DEFINE => $define,
	      INC => $inc,
	      LIBS => "$libpath -ljs",
	      VERSION_FROM => 'JS.pm',
	      %extras,);

__END__
