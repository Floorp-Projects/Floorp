#!perl -w
package MozillaBuildCore;

require 5.004;
require Exporter;

use strict;
use vars qw( @ISA @EXPORT );

# perl includes
use Cwd;

# homegrown
use Moz;
use MozJar;
use MozBuildFlags;
use MozBuildUtils;
use MozillaBuildList;


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
        my($profilerlibpath) = CodeWarriorLib::getCodeWarriorPath("MacOS Support:Profiler:Profiler Common:ProfilerLib");
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
    my($universal_interfaces) = CodeWarriorLib::getCodeWarriorPath("MacOS Support:Universal:Interfaces:CIncludes:");
    if (-e ($universal_interfaces . "ControlDefinitions.h")) {
        $main::UNIVERSAL_INTERFACES_VERSION = 0x0330;
    }

    #// Rename IC SDK folder in the Mac OS Support folder
    my($ic_sdk_folder) = CodeWarriorLib::getCodeWarriorPath("MacOS Support:ICProgKit2.0.2");
    if( -e $ic_sdk_folder)
    {
        my($new_ic_folder_name) = CodeWarriorLib::getCodeWarriorPath("MacOS Support:(ICProgKit2.0.2)");
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
#// RunBuild
#//--------------------------------------------------------------------------------------------------
sub RunBuild($$$$)
{
    my($do_pull, $do_build, $build_flags_file, $build_prefs) = @_;
    
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
                     $build_flags_file,
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
        Checkout();
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
