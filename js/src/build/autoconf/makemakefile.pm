package makemakefile;

# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1999-2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Steve Lamm <slamm@netscape.com>
#   Joey Armstrong <joey@mozilla.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

##----------------------------##
##---] CORE/CPAN INCLUDES [---##
##----------------------------##
use strict;
use warnings;
# use feature 'state'; 5.10+ not available everywhere

##-------------------##
##---]  EXPORTS  [---##
##-------------------##
our $VERSION = qw(2.0);
use Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(dirname_legacy
                 getConfig getDepth getRelPath getObjDir getTopDir mkdirr
                 getExclusions
                 run_config_status
                 updateMakefiles
                 update_makefiles_legacy
                 );

##--------------------##
##---]  INCLUDES  [---##
##--------------------##
use Cwd;
use Cwd     qw{abs_path};
use FindBin;
use File::Basename;
use File::Copy;

##-------------------##
##---]  GLOBALS  [---##
##-------------------##
umask 0;
my $cwd = Cwd::abs_path('.');
my %argv;


###########################################################################
## Intent: Helper function, retrieve contents of a file with error checking
## -----------------------------------------------------------------------
## Args:
##   scalar   path to input file
## Returns:
##   array    contents of the given file
##   $@       set on error
###########################################################################
sub cat
{
    my $fyl = shift || '';
    $@ = '';
    my @data;

    local *FYL;
    if (!open(FYL, $fyl))
    {
        $@ = "open($fyl) failed: $!";
    }
    else
    {
        @data = <FYL>;
        close(FYL);
    }
    return @data;
} # cat

###########################################################################
## Intent: Return directory path for a given argument
## -----------------------------------------------------------------------
## -----------------------------------------------------------------------
## Todo:
##   o Check if function can be replaced by File::Basename::dirname()
###########################################################################
sub dirname_legacy
{
    my $str = (@_ && defined($_[0])) ? shift : '';
    return $str =~ /(.*)\/.*/ ? "$1" : '.';
}

###########################################################################
## Intent: Given a list of makefile paths recursively create all
##         directories between file and the root
## -----------------------------------------------------------------------
## Args:
##   array   A list of makefiles
##   fargs   Function arguments
##     mode  Filesystem mode used for directory creation
## Returns:
##   $@      Set on error
##   0       on success
## -----------------------------------------------------------------------
## Note:
##   Reporting directory creation can be enabled by the --verbose
##   command line argument.
###########################################################################
sub mkdirr
{
    my %fargs = (@_ && ref($_[$#_])) ? %{ (pop) } : ();
    my $mode = $fargs{mode} || 0755;
    my $verbose = $main::argv{verbose} || 0;
    $@ = '' unless ($fargs{recursive});
    $fargs{recursive} = 1;

    my @errors;
    push(@errors, $@) if ($@);
    foreach my $path (@_)
    {
        (my $dir = $path) =~ s%/?Makefile[^/]*$%%o;
        next unless (length($dir));
        next if (-e $dir);
        mkdirr( dirname($dir), \%fargs);
        eval{ File::Path::mkpath($dir, $verbose, 0755); };
        push(@errors, $@) if ($@);
    }
    $@ = join("\n", @errors);
    return $@ ? 0 : 1;
} # mkdirr

###########################################################################
## Intent: Read in configure values and return a hash of key/value pairs
## -----------------------------------------------------------------------
## Args:
##   fargs  Function arguments
##     reset   clear value storage and repopulate
## Returns:
##   hash  configure data to use for makefile substitutions
## -----------------------------------------------------------------------
## Todo: wrapper for reading config* and run_config_status
###########################################################################
my %_CONFIG_; # todo: state %config; w/5.10
sub getConfig
{
    my %fargs = (@_ && ref($_[$#_]) eq 'HASH') ? %{ (pop) } : ();
    if ($fargs{reset})
    {
        %_CONFIG_ = ();
        shift;
    }

    #my $ac_file_in    = "$ac_given_srcdir/${ac_file}.in";
    #my $ac_dir        = dirname_legacy($ac_file);
    #my $ac_dots       = '';
    #my $ac_dir_suffix = '';
    #my $srcdir        = '.';
    #my $top_srcdir    = '.';
    unless (%_CONFIG_)
    {
        while (@_)
        {
            my ($k, $v) = splice(@_, 0, 2);
            $_CONFIG_{$k} = $v;
        }
    }

    return %_CONFIG_;
} # getConfig

###########################################################################
## Intent: Determine path depth between leaf and root directory.
##   o DEPTH= may be set by makefile content
##   o DEPTH= may be set by Makefile in a parent
##   o Manually determine by relpath form leaf to sandbox top
## -----------------------------------------------------------------------
## Args:
##   scalar  Path to makefile or directory to determine DEPTH for
## Returns:
##   scalar  Relative path from leaf to root directory
## -----------------------------------------------------------------------
###########################################################################
sub getDepth($)
{
    my $fyl = shift || '';

    my @path = split(m%/%o, $fyl);
    pop(@path) if ('Makefile' eq substr($path[$#path], 0, 8));
    my $depth;
    my @depth;

    my $top = getTopDir();
    my @top = split(m%/%o, $top);
    my @pathNoTop = @path;
    splice(@pathNoTop, 0, scalar(@top));

    SEARCH:
    while (@path)
    {
        ## Search for a file containing DEPTH=../..
        foreach my $fyl ( qw{Makefile.in Makefile} )
        {
            my $path = join('/', @path, $fyl);
            local *FYL;
            if (!open(FYL, $path)) {} # NOP
            elsif (my @tmp = map{ /^\s*DEPTH\s*=\s*([\.\/]+)/o ? $1 : () } <FYL>)
            {
                $depth = join('/', @depth, shift @tmp);
                last SEARCH;
            }
            close(FYL);
        }
        pop @path;
        pop @pathNoTop;

        if (0 == scalar(@pathNoTop))
        {
            $depth = join('/', @depth);
            last;
        }
        
        ## Construct path manually
        push(@depth, '..');
    }
    return $depth;
} # getDepth

###########################################################################
## Intent: Read in the exclusion file
###########################################################################
sub getExclusions
{
    my $file = shift || '';
    
    return () if ($main::argv{'no-exclusions'});

    my %exclude;
    if ($file)
    {
        my @data = cat($file);
        foreach (@data)
        {
            next unless ($_);
            next if (/^\s*\#/o);
            next unless (m%/%);
            chomp;
            $exclude{$_}++;
        }
    }
    return %exclude;
} # getExclusions

###########################################################################
## Intent: Given the path to a makefile beneath either src or obj
##         derive the relative path prefix between makefile and root.
###########################################################################
sub getRelPath
{
    my $path0 =  shift;
    my $abspath;

    # Determine type and orientation
    my $name = basename($path0);
    my $haveMF = ($name eq 'Makefile.in') ? 1
        : ($name eq 'Makefile') ? -1
        : 0
        ;

    ####################################################
    ## Prep work: form a relative path with ../ removed
    ####################################################
    my $top = getTopDir();
    my $obj = getObjDir();
    ## If the same Makefile will be created alongside Makefile.in
    my $topQM = quotemeta($top);
    my $objQM = quotemeta($obj);

    if ('..' eq substr($path0, 0, 2))
    {
        my @cwd = split(m%/%, $cwd);
        my @pth = split(m%/%, $path0);
        while (@pth && $pth[0] eq '..')
        {
            pop(@cwd);
            shift @pth;
        }
        $path0 = join('/', @cwd, @pth);
        $abspath = $path0;
    }

    if ('/' eq substr($path0, 0, 1))
    {
        $path0 =~ s%^$objQM\/?%%;
        $path0 =~ s%^$topQM\/?%%;
    }

    #######################################################################
    ## Build a list of directories to search.  Input source will be one
    ## of path to Makefile.in, path to Makefile, directory, file within
    ## a directory or relative path from cwd.
    #######################################################################
    my @subdirs;
    my $path = (0 == $haveMF) ? $path0 : dirname($path0);
    push(@subdirs, $path); # containing directory
    push(@subdirs, dirname($path)) if (0 == $haveMF && -f $path); # Arg is file within a directory
    push(@subdirs, $cwd);  # relative to pwd

    # obj - path to generated makefile
    # top - path to Makefile.in source template
    my @prefixes = ('/' ne substr($path0, 0, 1))
        ? (&getTopDir, &getObjDir)
        : ()
        ;

  ON_SAFARI:
    for my $prefix (@prefixes)
    {
        next unless ($prefix); # no command line not passed
        foreach my $subdir (@subdirs)
        {
            foreach my $mf ('Makefile.in', 'Makefile')
            {
                my $path = join('/', $prefix, $subdir, $mf);
                if (-e $path)
                {
                    $name = $mf;
                    $haveMF = ($mf eq 'Makefile.in') ? 1 : -1;
                    $abspath = $path;
                    last ON_SAFARI;
                }
            }
        }
    }

    #######################################################################
    ## Generated makefile does not yet exist or path is invalid.
    ## Should this conditon be handled to detect non-existent Makefile.in:
    ##   Makefile.am => Makefile.in => Makefile but Makefile.in
    #######################################################################
    if (!$abspath && -1 == $haveMF && $obj)
    {
        $abspath = ('/' eq substr($path0, 0, 1)) 
            ? $path0
            : join('/', $obj, $path0)
            ;
    }

    ########################################################
    ## If --top and/or --obj specified extract relative path
    ########################################################
    my $relpath;
    if (! $abspath)
    {
        # Error, fall through
    }
    elsif (1 == $haveMF) # Makefile.in
    {
        ## err w/o --top
        (my $tmp = $abspath) =~ s%^$topQM/?%%;
        $relpath = dirname($tmp) unless ($tmp eq $abspath);
    }
    elsif (-1 == $haveMF) # Makefile
    {
        ## err w/o --obj
        (my $tmp = $abspath) =~ s%^$objQM/?%%;
        $relpath = dirname($tmp) unless ($tmp eq $abspath);
    }

    $relpath ||= '';
    $relpath =~ s%/./%/%og; # filter ./

    $@ = ($relpath) ? '' : "ERROR($path0): Unable to locate sources";
    return $relpath || '';
} # getRelPath

###########################################################################
## Intent: Determine sandbox root from script startup directory
## -----------------------------------------------------------------------
## Args:
##    _set_    optional, if passed use the given value as path
##    _reset_  clear cached directory path to reassign
## Returns:
##   scalar - absolute path to the sandbox root directory
## -----------------------------------------------------------------------
###########################################################################
my $gtd_dir;
sub getTopDir
{
    if (@_) # testing override
    {
        $gtd_dir = abs_path($_[1] || '.') if ($_[0] eq '_set_');
        $gtd_dir = ''    if ($_[0] eq '_reset_');
    }

    unless ($gtd_dir)
    {
        ## Set by command line
        if ($main::argv{top})
        {
            $gtd_dir = $main::argv{top};
        }
        else
        {
            my $path = abs_path($FindBin::RealBin);
            my @path = split(m%/%o, $path);
            ## --2 memory/mozalloc/Makefile.in
            ## --3 was this for FindBin::Script ?
            splice(@path, -2);
            $gtd_dir = join('/', @path);
        }
    }
    return $gtd_dir;
} # getTopDir

###########################################################################
## Intent: Determine path to MOZ_OBJDIR/object directory
## -----------------------------------------------------------------------
## Args:
##   _set_    optional testing arg, if passed re-compute cached value
## Returns:
##   scalar - absolute path to the sandbox object directory
## -----------------------------------------------------------------------
###########################################################################
my $god_dir;
sub getObjDir
{
    if (@_) # testing override
    {
        if ($_[0] eq '_reset_')
        {
            $god_dir = '';
            shift;
        }
        elsif ($_[0] eq '_set_')
        {
            shift;
            my $path = $_[0] || '.';
            $god_dir = abs_path($path);
            shift;
        }
    }

    ## extract $obj from given path
    unless ($god_dir)
    {
        if ($main::argv{obj})
        {
            $god_dir = $main::argv{obj};
        }
        elsif (@_ && 'Makefile' eq substr($_, -8))
        {
            $god_dir = abs_path(shift);
        }
        else # assume we are sitting in moz_objdir
        {
            $god_dir = abs_path('.');
        }
    }

    return $god_dir;
} # getObjDir

###########################################################################
## Intent: Generate Makefile from a given Makefile.in template
## -----------------------------------------------------------------------
## Args:
##   scalar  Relative path to a directory containing a makefile
##   fargs   Hash ref of function arguments.
##     obj     Absolute path to MOZ_OBJ/a destination directory
##     top     Absolute path to the sandbox root
## Returns:
##    $@     Set on error
##    scalar
##      1     True if the makefile was updated
##      0     Otherwise
##      badtokens - If the makefile contains unexpandable @token@ strings
## -----------------------------------------------------------------------
###########################################################################
sub updateMakefiles
{
    my %fargs = (@_ && ref($_[$#_])) ? %{ (pop) } : ();
    local $_;
    $@ = '';

    my $top = $fargs{top};
    my $obj = $fargs{obj};

    my $relpath = shift || '';
    my $src = join('/',  $top, $relpath, 'Makefile.in');
    my $depth = getDepth($src);

    my @src = cat($src);
    return 0 if ($@);

    my $dst = join('/', $obj, $relpath, 'Makefile');
    my @dst = cat($dst);
    $@ = '';

    my $dstD = dirname($dst);
    mkdirr($dstD);
    return 0 if ($@);

    my %data =
        ( getConfig(),
          depth      => $depth,
          srcdir     => join('/', $top, $relpath),
          top_srcdir => $top,
        );

    my $line = 0;
    my @data;
    while (scalar @src)
    {
        $line++;
        $_ = shift(@src);

        ## Expand embedded @foo@
        while (/\@[^\@\s\$]+\@/go)
        {
            my $end = pos($_);
            my $val = $&;
            my $len = length($val);
            $val =~ s/^\@\s*//o;
            $val =~ s/\s*\@$//o;

            ## Identify expansions to see if we can avoid shell overhead
            if (!defined $data{$val} && !$argv{'no-badtokens'})
            {
                if (1) # warnings
                {
                    print STDERR "WARNING: token $val not defined\n";
                    print STDERR "   line $line, src: $src\n";
                }
                return 'badtokens';
            }

            # Insert $(error txt) makefile macros for invalid tokens
            my $val1 = defined($data{$val})
                ? $data{$val}
                : "\$(error $FindBin::Script: variable ${val} is undefined)"
                ;
            substr($_, ($end-$len), $len, $val1);
        }
        push(@data, $_);
    }

    if (("@data" eq "@dst") && scalar(@data))
    {
        print "Skipping up2date makefile: $dst\n" if ($argv{verbose});
    }
    else
    {
        my $action = (scalar @dst) ? 'Updating' : 'Creating';
        print "$action makefile: $dst\n";

        my $tmp = join('.', $dst, "tmp_$$");
        if (!open(FYL, "> $tmp"))
        {
            $@ = "open($tmp) failed: $!";
        }
        else
        {
            print FYL @data;
            close(FYL);

            ## Install the new makefile
            File::Copy::move($tmp, $dst)
                || ($@ = "move($tmp, $dst) failed: $!");
        }
    }

    return $@ ? 0 : 1;
} # updateMakefiles

# Output the makefiles.
#
sub update_makefiles_legacy {
  my ($ac_given_srcdir, $pac_given_srcdir, @makefiles) = @_;
  my $debug = $main::argv{debug} || 0;
  my $pwdcmd = ($^O eq 'msys') ? 'pwd -W' : 'pwd';
  my @unhandled=();

  my @warn;

  my $ac_file;
  foreach $ac_file (@makefiles) {
    my $ac_file_in    = "$ac_given_srcdir/${ac_file}.in";
    my $ac_dir        = dirname_legacy($ac_file);
    my $ac_dots       = '';
    my $ac_dir_suffix = '';
    my $srcdir        = '.';
    my $top_srcdir    = '.';

    # Determine $srcdir and $top_srcdir
    #
    if ($ac_dir ne '.') {
      $ac_dir_suffix = "/$ac_dir";
      $ac_dir_suffix =~ s%^/\./%/%;
      $ac_dots = $ac_dir_suffix;
      # Remove .. components from the provided dir suffix, and
      # also the forward path components they were reversing.
      my $backtracks = $ac_dots =~ s%\.\.(/|$)%%g;
      while ($backtracks--) {
        $ac_dots =~ s%/[^/]*%%;
      }
      $ac_dots =~ s%/[^/]*%../%g;
    }
    if ($ac_given_srcdir eq '.') {
      if ($ac_dots ne '') {
        $top_srcdir = $ac_dots;
        $top_srcdir =~ s%/$%%;
      }
    } elsif ($pac_given_srcdir =~ m%^/% or $pac_given_srcdir =~ m%^.:/%) {
      $srcdir     = "$pac_given_srcdir$ac_dir_suffix";
      $top_srcdir = "$pac_given_srcdir";
    } else {
      if ($debug) {
              print "ac_dots       = $ac_dots\n";
        print "ac_dir_suffix = $ac_dir_suffix\n";
      }
      $srcdir     = "$ac_dots$ac_given_srcdir$ac_dir_suffix";
      $top_srcdir = "$ac_dots$ac_given_srcdir";
    }

    if ($debug) {
      print "ac_dir     = $ac_dir\n";
      print "ac_file    = $ac_file\n";
      print "ac_file_in = $ac_file_in\n";
      print "srcdir     = $srcdir\n";
      print "top_srcdir = $top_srcdir\n";
      print "cwd        = " . `$pwdcmd` . "\n";
    }

    # Copy the file and make substitutions.
    #    @srcdir@     -> value of $srcdir
    #    @top_srcdir@ -> value of $top_srcdir
    #
    if (-e $ac_file) {
      next if -M _ < -M $ac_file_in;  # Next if Makefile is up-to-date.
      warn "updating $ac_file\n";
    } else {
      warn "creating $ac_file\n";
    }

    mkdirr(dirname($ac_file));

    open INFILE, "<$ac_file_in" or do {
      warn "$0: Cannot read $ac_file_in: No such file or directory\n";
      next;
    };
    open OUTFILE, ">$ac_file" or do {
      warn "$0: Unable to create $ac_file\n";
      next;
    };

    while (<INFILE>) {
      s/\@srcdir\@/$srcdir/g;
      s/\@top_srcdir\@/$top_srcdir/g;

      if (/\@[_a-zA-Z]*\@/) {
        #warn "Unknown variable:$ac_file:$.:$_";
        push @unhandled, $ac_file;
        last;
      }
      print OUTFILE;
    }
    close INFILE;
    close OUTFILE;
  }
  return @unhandled;
} # update_makefiles_legacy

###########################################################################
## Intent: Invoke config.status for unknown makefiles to create
##         directory hierarchy for the tree.
## -----------------------------------------------------------------------
## Args:
##   array   an optional list of makefiles to process
## Returns:
##   0    on success
##   $#   set on error
## -----------------------------------------------------------------------
## Note: Is this function needed anymore ?  Undefined tokens should fail
##   at time of expansion rather than having to source config.status.
##   Also config.status could be parsed to define values and avoide the
##   shell overhead altogether.
###########################################################################
sub run_config_status {
  my @unhandled = @_;

  # Run config.status with any unhandled files.
  #
  my @errors;
  if (@unhandled) {
    local $ENV{CONFIG_FILES}= join ' ', @unhandled;

    my $conf = 'config.status';
    if (! -e $conf) # legacy behavior, warn rather than err
    {
        my $cwd = cwd();
        my $err = "$FindBin::Script ERROR: Config file $conf does not exist, cwd=$cwd";
        push(@errors, $err);
    }
    elsif (0 != system("./config.status"))
    {
        my $cwd = cwd();
        push(@errors, "config.status failed \$?=$?, \$!=$!, cwd: $cwd");
    }
  }
  $@ = join("\n", @errors);

  ## Legacy behavior: config.status problems are not fatal {yet}.
  ## Display warning since caller will not be calling die.
  warn $@ if ($@ && $argv{'no-warnings'});
  return $@ ? 1 : 0;
}

1;
