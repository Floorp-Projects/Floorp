#!perl -w
package Moz::BuildCore;

require 5.004;
require Exporter;

use strict;
use vars qw( @ISA @EXPORT );

# perl includes
use Cwd;
use File::Basename;

# homegrown
use Moz::Moz;
use Moz::Jar;
use Moz::BuildFlags;
use Moz::BuildUtils;
use Moz::CodeWarriorLib;

use MozillaBuildList;   # eventually, this should go away, and be replaced by data input


@ISA        = qw(Exporter);
@EXPORT     = qw(RunBuild);


#//--------------------------------------------------------------------------------------------------
#// DoPrebuildCheck
#//
#// Check the build tools etc before running the build.
#//--------------------------------------------------------------------------------------------------
sub DoPrebuildCheck()
{
    SanityCheckJarOptions();

    # launch codewarrior and persist its location. Have to call this before first
    # call to getCodeWarriorPath().
    my($ide_path_file) = $main::filepaths{"idepath"};
    $ide_path_file = full_path_to($ide_path_file);
    LaunchCodeWarrior($ide_path_file);
}

#//--------------------------------------------------------------------------------------------------
#// Configure Build System
#//--------------------------------------------------------------------------------------------------

sub GenBuildSystemInfo()
{
    # always rebuild the configuration program.
    BuildProjectClean(":mozilla:build:mac:tools:BuildSystemInfo:BuildSystemInfo.mcp", "BuildSystemInfo");

    # delete the configuration file.
    unlink(":mozilla:build:mac:BuildSystemInfo.pm");
    
    # run the program.
    system(":mozilla:build:mac:BuildSystemInfo");

    # wait for the file to be created.
    while (!(-e ":mozilla:build:mac:BuildSystemInfo.pm")) { WaitNextEvent(); }
    
    # wait for BuildSystemInfo to finish, so that we see correct results.
    while (IsProcessRunning("BuildSystemInfo")) { WaitNextEvent(); }

    # now, evaluate the contents of the file.
    open(F, ":mozilla:build:mac:BuildSystemInfo.pm");
    while (<F>) { eval; }
    close(F);
}

#//--------------------------------------------------------------------------------------------------
#// Make library aliases
#//--------------------------------------------------------------------------------------------------

sub MakeLibAliases()
{
    my($dist_dir) = GetBinDirectory();

    #// ProfilerLib
    if ($main::PROFILE)
    {
        my($profilerlibpath) = Moz::CodeWarriorLib::getCodeWarriorPath("MacOS Support:Profiler:Profiler Common:ProfilerLib");
        MakeAlias("$profilerlibpath", "$dist_dir"."Essential Files:");
    }
}

#//--------------------------------------------------------------------------------------------------
#// Regenerate DefinesOptions.h if necessary
#//
#//--------------------------------------------------------------------------------------------------

sub UpdateConfigHeader($)
{
    my($config_path) = @_;
    
    my($config, $oldconfig) = ("", "");
    my($define, $definevalue, $defines);
    my($k, $l,);

    foreach $k (keys(%main::options))
    {
        if ($main::options{$k})
        {
            foreach $l (keys(%{$main::optiondefines{$k}}))
            {
                $my::defines{$l} = $main::optiondefines{$k}{$l};
                print "Setting up my::defines{$l}\n";
            }
        }
    }

    my $config_headerfile = current_directory().$config_path;
    if (-e $config_headerfile)
    {
        open(CONFIG_HEADER, "< $config_headerfile") || die "$config_headerfile: $!\n";
        my($line);
        while ($line = <CONFIG_HEADER>)
        {
            $oldconfig .= $line;
            if ($line =~ m/#define (.*) (.*)\n/)
            {
                $define = $1;
                $definevalue = $2;
                if (exists ($my::defines{$define}) and ($my::defines{$define} == $definevalue))
                {
                    delete $my::defines{$define};
                    $config .= $line;
                }
            }
        }
        close(CONFIG_HEADER);
    }

    if (%my::defines)
    {
        foreach $k (keys(%my::defines))
        {
            $config .= "#define " . $k . " " . $my::defines{$k} . "\n";
        }
    }

    if (($config ne $oldconfig) || (!-e $config_headerfile))
    {
        printf("Writing new DefinesOptions.h\n");
        open(CONFIG_HEADER, "> $config_headerfile") || die "$config_headerfile: $!\n";
        MacPerl::SetFileInfo("CWIE", "TEXT", $config_headerfile);
        print CONFIG_HEADER ($config);
        close(CONFIG_HEADER);
    }
}


#//--------------------------------------------------------------------------------------------------
#// ConfigureBuildSystem
#//
#// defines some build-system configuration variables.
#//--------------------------------------------------------------------------------------------------
sub ConfigureBuildSystem()
{
    #// In the future, we may want to do configurations based on the actual build system itself.
    #// GenBuildSystemInfo();
        
    #// For now, if we discover a newer header file than existed in Universal Interfaces 3.2,
    #// we'll assume that 3.3 or later is in use.
    my($universal_interfaces) = Moz::CodeWarriorLib::getCodeWarriorPath("MacOS Support:Universal:Interfaces:CIncludes:");
    if (-e ($universal_interfaces . "ControlDefinitions.h")) {
        $main::UNIVERSAL_INTERFACES_VERSION = 0x0330;
    }

    #// Rename IC SDK folder in the Mac OS Support folder
    my($ic_sdk_folder) = Moz::CodeWarriorLib::getCodeWarriorPath("MacOS Support:ICProgKit2.0.2");
    if( -e $ic_sdk_folder)
    {
        my($new_ic_folder_name) = Moz::CodeWarriorLib::getCodeWarriorPath("MacOS Support:(ICProgKit2.0.2)");
        rename ($ic_sdk_folder, $new_ic_folder_name);
        # note that CodeWarrior doesn't descnet into folders with () the name
        print "Mozilla no longer needs the Internet Config SDK to build:\n  Renaming the 'ICProgKit2.0.2' folder to '(ICProgKit2.0.2)'\n";
    }

    printf("UNIVERSAL_INTERFACES_VERSION = 0x%04X\n", $main::UNIVERSAL_INTERFACES_VERSION);

    UpdateConfigHeader(":mozilla:config:mac:DefinesOptions.h");

    # alias required CodeWarrior libs into the Essential Files folder (only the Profiler lib now)
    MakeLibAliases();
}


#//--------------------------------------------------------------------------------------------------
#// CheckOutModule. Takes variable number of args; first two are required
#//--------------------------------------------------------------------------------------------------
sub CheckOutModule($$$$)
{
	my($session, $module, $revision, $date) = @_;

    my($result) = $session->checkout($module, $revision, $date);
    
    # result of 1 is success
    if ($result) { return; }
    
    my($checkout_err) = $session->getLastError();
    if ($checkout_err == 708) {
        die "Checkout was cancelled";
    } elsif ($checkout_err == 711) {
        print "Checkout of '$module' failed\n";
    }
}

#//--------------------------------------------------------------------------------------------------
#// getScriptFolder
#//--------------------------------------------------------------------------------------------------
sub getScriptFolder()
{
    return dirname($0);
}


#//--------------------------------------------------------------------------------------------------
#// Checkout
#//--------------------------------------------------------------------------------------------------
sub Checkout($)
{
    my($checkout_list) = @_;
    
    unless ( $main::pull{all} ) { return; }

#    assertRightDirectory();
    my($cvsfile) = AskAndPersistFile($main::filepaths{"sessionpath"});
    my($session) = Moz::MacCVS->new( $cvsfile );
    unless (defined($session)) { die "Error: Checkout aborted. Cannot create session file: $session" }

    # activate MacCVS
    ActivateApplication('Mcvs');

    my($checkout_file) = getScriptFolder().":".$checkout_list;
    local(*CHECKOUT_FILE);  
    open(CHECKOUT_FILE, "< $checkout_file") || die "Error: failed to open checkout list $checkout_file\n";
    while (<CHECKOUT_FILE>)
    {
        my($line) = $_;
        chomp($line);
    
        # ignore comments and empty lines
        if ($line =~ /^\#/ || $line =~ /^\s*$/) {
            next;
        }
    
        my($module) = "";
        my($revision) = "";
        my($date) = "";
        
        if ($line =~ /\s*([^\s]+)\s*\,\s*([^\s]+)\s*\,\s*(.+)\s*/)
        {
            $module = $1;
            $revision = $2;
            $date = $3;
        }
        elsif ($line =~ /\s*([\/\w]+)\s*\,\s*(\w+)\s*/)
        {
            $module = $1;
            $revision = $2;
        }
        elsif ($line =~ /\s*([^\s]+)\s*\,\s*,\s*(.+)\s*/)       
        {
            $module = $1;
            $date = $2;
        }
        elsif ($line =~ /\s*([\/\w]+)/)
        {
            $module = $1;
        }
        
        # print "Checking out '$module', '$revision', '$date'\n";
        CheckOutModule($session, $module, $revision, $date);
    }

    close(CHECKOUT_FILE);
}


#//--------------------------------------------------------------------------------------------------
#// RunBuild
#//--------------------------------------------------------------------------------------------------
sub RunBuild($$$$)
{
    my($do_pull, $do_build, $input_files, $build_prefs) = @_;
    
    # if we are pulling, we probably want to do a full build, so clear the build progress
    if ($do_pull) {
        ClearBuildProgress();    
    }
    
    # read local prefs, and the build progress file, and set flags to say what to build
    SetupBuildParams(\%main::pull,
                     \%main::build,
                     \%main::options,
                     \%main::optiondefines,
                     \%main::filepaths,
                     $input_files->{"buildflags"},
                     $build_prefs);

    # setup the build log
    SetupBuildLog($main::filepaths{"buildlogfilepath"}, $main::USE_TIMESTAMPED_LOGS);
    StopForErrors();
    
    if ($main::LOG_TO_FILE) {
        RedirectOutputToFile($main::filepaths{"scriptlogfilepath"});
    }
        
    # run a pre-build check to see that the tools etc are in order
    DoPrebuildCheck();
    
    if ($do_pull) {
        Checkout($input_files->{"checkoutdata"});
    }
    
    unless ($do_build) { return; }

    # create generated headers
    ConfigureBuildSystem();
    UpdateBuildNumberFiles();
    
    chdir($main::MOZ_SRC);
    BuildDist();
    
    chdir($main::MOZ_SRC);
    BuildProjects();
    
    # the build finished, so clear the build progress state
    ClearBuildProgress();    
    print "Build complete\n";
}

1;
