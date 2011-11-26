#!/usr/bin/env perl
###########################################################################
## Intent: Unit test to verify make-makefile.tpl
###########################################################################

##----------------------------##
##---] CORE/CPAN INCLUDES [---##
##----------------------------##
use strict;
use warnings;
#use feature 'state';  # 5.10+ not installed everywhere
use Getopt::Long;

use Cwd;
use Cwd         qw{abs_path};
use File::Basename;
use File::Copy;
use File::Path;
use File::Temp  qw{ tempdir };

use Test;
sub BEGIN { plan tests => 4 };
my @workdirs;
sub END { system("/bin/rm -fr @workdirs"); }  # cleanup behind interrupts

##-------------------##
##---]  EXPORTS  [---##
##-------------------##
use FindBin;
our $VERSION = qw(1.0);

##------------------##
##---] INCLUDES [---##
##------------------##
use FindBin;
use lib "$FindBin::RealBin/..";
use makemakefile;

##-------------------##
##---]  GLOBALS  [---##
##-------------------##
my %argv;

###########################################################################
## Intent: Create a temp sandbox populated with sources
## -----------------------------------------------------------------------
## Args:
##   array    a list of file paths to copy
## Returns:
##     $@     set on error
##   scalar   path to scratch sandbox
## -----------------------------------------------------------------------
###########################################################################
my $root; # state $root not available
sub createSandbox
{
    my @errors;

    unless ($root)
    {
        my @tmp = split(m%/%, $FindBin::RealBin);
        splice(@tmp, -3);
        $root = join('/', @tmp);
    }

    my $work = tempdir();
    push(@workdirs, $work);
    my @dirs = map{ join('/', $work, dirname($_)) } @_;
    mkdirr(@dirs);
    push(@errors, "createSandbox: $@") if ($@);

    foreach (@_)
    {
        ## Copy sources into the temp source directory
        my $src = join('/', $root, $_);
        my $dst = join('/', $work, $_);
        unless (copy($src, $dst))
        {
            push(@errors, "copy($src, $dst) failed: $!");
        }
    }
    print STDERR "createSandbox: $work\n" if ($main::argv{debug});
    $@ = join('', map{ "$_\n" } @errors);
    $work;
} # createSandbox

###########################################################################
## Intent: wrapper to run the make-makefile command.
## -----------------------------------------------------------------------
## Args:
##   array  command line arguments passed to make-makefile
## Returns:
##    array  command output
##    $@     set by shell exit status, empty string on success
##    $?     command shell exit status
###########################################################################
my $mm; # state $mm not available
sub makemakefile
{
    my %fargs = (@_ && ref($_[$#_])) ? %{ (pop) } : ();
    $mm ||= join('/', dirname($FindBin::Bin), 'make-makefile'); # cmd in parent of test/
    my $cmd = join(' ', $mm, @_);
    print "RUNNING: $cmd\n" if ($fargs{debug});
    my @out = `$cmd 2>&1`;
    print STDERR map{ "out> $_" } @out if ($argv{verbose});
    $@ = (0 == $?) ? '' : "Command failed: $cmd\n@out";
    @out;
} # makemakefile

###########################################################################
## Intent: Helper function, display the contents of a given sandbox
## -----------------------------------------------------------------------
## Args:
##   scalar   Path to sandbox
## Returns:
##   none
## -----------------------------------------------------------------------
###########################################################################
sub find_ls
{
    my $path = shift || '';

    # Assuming dot contributes to cryptic problems
    die "find_ls: a path is required" unless ($path);

    my $cmd = "find $path -ls";
    print "\nRunning: $cmd\n";
    print '=' x 75, "\n";
    print `$cmd`;
} # myls

###########################################################################
## Intent: Verify make-makefile is able to digest paths and generate
##         makefiles when object directory is a child of top.
###########################################################################
sub check_makemakefile
{
    my $work = createSandbox
        (
         'memory/mozalloc/Makefile.in',
         'toolkit/system/windowsproxy/Makefile.in',
         'toolkit/crashreporter/google-breakpad/src/client/Makefile.in',
        );


    my $workdir = createSandbox();
    my $top = $workdir;
    chdir $top;

    my $objA = 'obj-arch-dir';
    my $obj = join('/', $top, $objA);

    # getTopDir()
    local $main::argv{top} = $work;
    local $main::argv{obj} = $obj;
    getObjDir('_reset_');

    my @root = split(m%/%, $FindBin::RealBin);
    splice(@root, -3);
    my $root = join('/', @root);
    my @args =
        (

         [
          banner => "--top and --obj are impled, generate Makefile",
             rel => 'memory/mozalloc',
             cmd => join(' ',
                         '--top', $top,
                         '--obj', $obj,
                         'memory/mozalloc/Makefile',
                         ),
         ],

         [
          banner => "--top and abs(obj) passed",
             rel => "toolkit/system/windowsproxy",
             cmd => join(' ',
                         '--top', $top,
                         "$obj/toolkit/system/windowsproxy/Makefile",
                         ),
             exp => "$obj/toolkit/system/windowsproxy/Makefile",
            skip => 1, #
         ],


         [
          banner => "--obj and abs(top) passed",
             rel => "toolkit/crashreporter/google-breakpad/src/client",
             cmd => join(' ',
                         '--obj', $obj,
                         "$top/toolkit/crashreporter/google-breakpad/src/client/Makefile.in",
                         ),
             exp => "$top/toolkit/crashreporter/google-breakpad/src/client/Makefile.in",
          skip => 1, #
         ],

         );

    foreach (@args)
    {
        my %rec = @{ $_ };
        next if ($rec{skip});
        next unless ($rec{rel});

        my $srcR = join('/', $top, $rec{rel});
        my $dstR = join('/', $obj, $rec{rel});

        my $src = join('/', $top, $rec{rel}, 'Makefile.in');
        my $dst = join('/', $obj, $rec{rel}, 'Makefile');

        # Use distinct sources to avoid cleanup overhead between tests
        die "Test source already used: $dstR" if (-d $dstR);

        ## Copy sources into the temp source directory
        my $rootR = join('/', $root, $rec{rel});
        my $rootS = join('/', $root, $rec{rel}, 'Makefile.in');
        File::Path::mkpath($srcR, 0, 0700);
        copy($rootS, $src) or die "copy($rootS, $src) failed: $!";

        die "source does not exist: $src" unless (-e $src);

        ######################
        ## Generate and verify
        ######################
        print STDERR "RUNNING: $rec{banner}\n" if ($argv{debug});
        my @errs;
        makemakefile('--enhanced', $rec{cmd}, {verbose=>1});
        if ($@)
        {
            push(@errs, "\$@ should not be set: $@\n");
        }
        elsif (! -e $dst)
        {
            push(@errs, "Generated makefile does not exist: $dst, banner: $rec{banner}\n");
        }

        ok(scalar(@errs), 0, "Errors detected:\n" . join("    $_", @errs));
        find_ls($top) if (@errs);
    }

} # check_makemakefile

###########################################################################
## Intent: Verify make-makefile is able to digest paths and generate
##         makefiles when top/MOZ_OBJDIR are not parent/child directories
## ---------------------------------------------------------------------------
## Args:
##   none
## Returns:
##   none
## ---------------------------------------------------------------------------
###########################################################################
sub check_makemakefile_distinct
{
    my $workdir = createSandbox();
#    my $workdir = tempdir();

    ###############################################
    ## Now update when top/obj are not parent/child
    ###############################################
    my $top = join('/', $workdir, 'top');
    my $obj = join('/', $workdir, 'obj');

    $main::argv{top} = $top;
    $main::argv{obj} = $obj;  # test afterward, using undef ?

    my @sbxroot = split(m%/%, $FindBin::RealBin);
    splice(@sbxroot, -2);
    my $sbxroot = join('/', @sbxroot);

    ## Copy in a makefile template to to convert
    File::Path::mkpath(["$top/memory/mozalloc"], 0, 0700);
    copy("$sbxroot/memory/mozalloc/Makefile.in", "$top/memory/mozalloc/Makefile.in");


    # work/memory/mozalloc/Makefile.in

    my @args =
        (
         [
          banner => '--top and --obj are distinct [1]',
             cmd => "--obj $obj memory/mozalloc/Makefile",
             exp => "$obj/memory/mozalloc/Makefile",
         ],

         [
          banner => "--top and --obj are distinct [2]",
             cmd => "--top $top memory/mozalloc/Makefile.in",
             exp => "$obj/memory/mozalloc/Makefile",
            skip => 1,  # test problem: top != obj
         ],

         [
          banner => "--top and --obj are distinct [3]",
             cmd => join(' ',
                         "--top $top",
                         "--obj $obj",
                         "memory/mozalloc/Makefile.in",
                         ),
             exp => "$obj/memory/mozalloc/Makefile",
            skip => 1,  # test problem: top != obj
         ],
        );


    foreach (@args)
    {
        my %rec = @{ $_ };
        print STDERR "banner: $rec{banner}\n" if ($argv{debug});
        next if $rec{skip};

        unlink $rec{exp};
        makemakefile('--enhanced', $rec{cmd});

        my @errs;
        if ($@)
        {
            push(@errs, "\$@ should not be set: $@\n");
        }
        elsif (! -e $rec{exp})
        {
            push(@errs, "Makefile does not exist: $rec{exp}\n");
        }
        ok(scalar(@errs), 0, "Errors detected:\n" . join("    $_", @errs));
    }

} # check_makemakefile_distinct

###########################################################################
## Intent: Verify legacy behavior, invoke make-makefile when cwd is
##         a subdirectory beneath MOZ_OBJDIR.
## -----------------------------------------------------------------------
## Args:
##   none
## Returns:
##   none
## -----------------------------------------------------------------------
###########################################################################
sub check_makemakefile_legacy
{
    my $work = createSandbox
        (
         'memory/mozalloc/Makefile.in',
         'parser/htmlparser/tests/mochitest/html5lib_tree_construction/Makefile.in',
        );

    my $obj = join('/', $work, 'obj');
    mkdir $obj;

    my @args =
        (
         {
             banner => '-t path -d dot',
                cwd => $obj,
                cmd => "-t $work -d . memory/mozalloc/Makefile",
                exp => "$obj/memory/mozalloc/Makefile",
               skip => 0,
         },
         
         {
             banner => '-t path -d relpath',
                cwd => join('/', $obj, 'parser/htmlparser/tests/mochitest'),
                cmd => "-t $work -d ../../../.. html5lib_tree_construction/Makefile",
                exp => "$obj/parser/htmlparser/tests/mochitest/html5lib_tree_construction/Makefile",
               skip => 0,
         },
        );

    foreach (@args)
    {
        my %rec = %{ $_ };
        next if ($rec{skip});

        ## make-make while sitting in $objdir
        mkdirr($rec{cwd});
        chdir $rec{cwd} || die "chdir $rec{cwd} failed; $!";

        makemakefile($rec{cmd});
        my @errs;
        if ($@)
        {
            push(@errs, "make-makefile $rec{cmd} failed: $@");
        }
        elsif (! -e $rec{exp})
        {
            push(@errs, "generated makefile does not exist: $rec{exp}");
        }
        ok(scalar(@errs), 0, "Errors detected: @errs");
        find_ls($work) if (@errs);
    }
    chdir $FindBin::RealBin;
} # check_makemakefile_legacy

###########################################################################
## Intent: Smoke tests for the unittests module
###########################################################################
sub smoke
{
    print STDERR "Running test: smoke()\n" if ($argv{debug});
} # smoke()

###########################################################################
## Intent: Intitialize global test objects and consts
###########################################################################
sub init
{
    print "Running: init()\n" if ($argv{debug});
#    testplan(24, 0);
} # init()

##----------------##
##---]  MAIN  [---##
##----------------##
unless(GetOptions(\%argv,
                  qw(
                     debug|d
                     manual
                     test=s@
                     verbose
                     )))
{
    print "USAGE: $0\n";
    print "  --debug    Enable script debug mode\n";
    print "  --manual   Also run disabled tests\n";
    print "  --smoke    Run smoke tests then exit\n";
    print "  --test     Run a list of tests by function name\n";
    print "  --verbose  Enable script verbose mode\n";
    exit 1;
}

init();
smoke();

check_makemakefile();
check_makemakefile_distinct();
check_makemakefile_legacy();
