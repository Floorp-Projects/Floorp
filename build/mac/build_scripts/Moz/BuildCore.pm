#!perl -w
package Moz::BuildCore;

require 5.004;
require Exporter;

use strict;
use vars qw( @ISA @EXPORT );

# perl includes
use Cwd;
use POSIX;
use Time::Local;
use File::Basename;
use LWP::Simple;

# homegrown
use Moz::Moz;
use Moz::Jar;
use Moz::BuildFlags;
use Moz::BuildUtils;
use Moz::CodeWarriorLib;

# use MozillaBuildList;   # eventually, this should go away, and be replaced by data input


@ISA        = qw(Exporter);
@EXPORT     = qw(
                    RunBuild
                );


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
#// GenBuildSystemInfo
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
        # note that CodeWarrior doesn't descend into folders with () the name
        print "Mozilla no longer needs the Internet Config SDK to build:\n  Renaming the 'ICProgKit2.0.2' folder to '(ICProgKit2.0.2)'\n";
    }

    printf("UNIVERSAL_INTERFACES_VERSION = 0x%04X\n", $main::UNIVERSAL_INTERFACES_VERSION);

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
        die "Error: Checkout was cancelled.\n";
    } elsif ($checkout_err == 911) {
        die "Error: CVS session settings are incorrect. Check your password, and the CVS root settings.\n";
    } elsif ($checkout_err == 703) {
        die "Error: CVS checkout failed. Unknown module, unknown tag, bad username, or other CVS error.\n";
    } elsif ($checkout_err == 711) {
        print "Checkout of '$module' failed.\n";
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
#// getScriptFolder
#//--------------------------------------------------------------------------------------------------
sub get_url_contents($)
{
    my($url) = @_;

    my($url_contents) = LWP::Simple::get($url);
    $url_contents =~ s/\r\n/\n/g;     # normalize linebreaks
    $url_contents =~ s/\r/\n/g;       # normalize linebreaks
    return $url_contents;
}

#//--------------------------------------------------------------------------------------------------
#// get_files_from_content
#//--------------------------------------------------------------------------------------------------
sub uniq
{
    my $lastval;
    grep(($_ ne $lastval, $lastval = $_)[$[], @_);
}


#//--------------------------------------------------------------------------------------------------
#// get_files_from_content
#//--------------------------------------------------------------------------------------------------
sub get_files_from_content($)
{
    my($content) = @_;
  
    my(@jscalls) = grep (/return js_file_menu[^{]*/, split(/\n/, $content));
    my $i;
  
    for ($i = 0; $i < @jscalls ; $i++)
    {
        $jscalls[$i] =~ s/.*\(|\).*//g;
        my(@callparams) = split(/,/, $jscalls[$i]);
        my ($repos, $dir, $file, $rev) = grep(s/['\s]//g, @callparams);
        $jscalls[$i] = "$dir/$file";
    }

  &uniq(sort(@jscalls));
}

#//--------------------------------------------------------------------------------------------------
#// getLastUpdateTime
#// 
#// Get the last time we updated. Return 0 on failure
#//--------------------------------------------------------------------------------------------------
sub getLastUpdateTime($)
{
    my($timestamp_file) = @_;
 
    my($time_string);
    
    local(*TIMESTAMP_FILE);  
    unless (open(TIMESTAMP_FILE, "< $timestamp_file")) { return 0; }

    while (<TIMESTAMP_FILE>)
    {
        my($line) = $_;
        chomp($line);

        # ignore comments and empty lines
        if ($line =~ /^\#/ || $line =~ /^\s*$/) {
            next;
        }
        
        $time_string = $line;
    }

    # get the epoch seconds
    my($last_update_secs) = $time_string;
    $last_update_secs =~ s/\s#.+$//;
    
    print "FAST_UPDATE found that you last updated at ".localtime($last_update_secs)."\n";
    
    # how long ago was this, in hours?
    my($gm_now) = time();
    my($update_hours) = 1 + ceil(($gm_now - $last_update_secs) / (60 * 60));
    
    return $update_hours;
}


#//--------------------------------------------------------------------------------------------------
#// saveCheckoutTimestamp
#// 
#// Create a file on disk containing the current time. Param is time(), which is an Epoch seconds
#// (and therefore in GMT).
#// 
#//--------------------------------------------------------------------------------------------------
sub saveCheckoutTimestamp($$)
{
    my($gm_secs, $timestamp_file) = @_;
        
    local(*TIMESTAMP_FILE);
    open(TIMESTAMP_FILE, ">$timestamp_file") || die "Failed to open $timestamp_file\n";
    print(TIMESTAMP_FILE "# time of last checkout or update, in GMT. Used by FAST_UPDATE\n");
    print(TIMESTAMP_FILE "$gm_secs \# around ".localtime()." local time\n");
    close(TIMESTAMP_FILE);

}

#//--------------------------------------------------------------------------------------------------
#// FastUpdate
#// 
#// Use Bonsai url data to update only those dirs which have new files
#// 
#//--------------------------------------------------------------------------------------------------
sub FastUpdate($$)
{
    my($modules, $timestamp_file) = @_;          # list of modules to check out

    my($num_hours) = getLastUpdateTime($timestamp_file);
    if ($num_hours == 0 || $num_hours > 170) {
        print "Can't fast_update; last update was too long ago, or never. Doing normal checkout.\n";
        return 0;
    }

    print "Doing fast update, pulling files changed in the last $num_hours hours\n";

    my($cvsfile) = AskAndPersistFile($main::filepaths{"sessionpath"});
    my($session) = Moz::MacCVS->new( $cvsfile );
    unless (defined($session)) { die "Error: Checkout aborted. Cannot create session file: $session" }

    # activate MacCVS
    ActivateApplication('Mcvs');
    
    my($checkout_start_time) = time();

    #print "Time now is $checkout_start_time ($checkout_start_time + 0)\n";
    
    my($this_co);
    foreach $this_co (@$modules)
    {
        my($module, $revision, $date) = ($this_co->[0], $this_co->[1], $this_co->[2]);        
        
        # assume that things pulled by date wont change
        if ($date ne "") {
            print "$module is pulled by date, so ignoring in FastUpdate.\n";
            next;
        }

        my($search_type) = "hours";
        my($min_date) = "";
        my($max_date) = "";
        my($url) = "http://bonsai.mozilla.org/cvsquery.cgi?treeid=default&module=${module}&branch=${revision}&branchtype=match&dir=&file=&filetype=match&who=&whotype=match&sortby=Date&hours=${num_hours}&date=${search_type}&mindate=${min_date}&maxdate=${max_date}&cvsroot=%2Fcvsroot";

        if ($revision eq "") {
            print "Getting list of checkins to $module from Bonsai...\n";
        } else {
            print "Getting list of checkins to $module on branch $revision from Bonsai...\n";
        }
        my(@files) = &get_files_from_content(&get_url_contents($url));

        if ($#files > 0)
        {
           my(@cvs_co_list);
        
           my($co_file);
           foreach $co_file (@files)
           {
               print "Updating $co_file\n";
               push(@cvs_co_list, $co_file);
           }

            my($result) = $session->update($revision, \@cvs_co_list);
            # result of 1 is success
            if (!$result) { die "Error: Fast update failed\n"; }
        } else {
            print "No files in this module changed\n";        
        }
    }
    
    saveCheckoutTimestamp($checkout_start_time, $timestamp_file);
    return 1;
}


#//--------------------------------------------------------------------------------------------------
#// Checkout
#//--------------------------------------------------------------------------------------------------
sub CheckoutModules($$$)
{
    my($modules, $pull_date, $timestamp_file) = @_;          # list of modules to check out

    my($start_time) = TimeStart();

#    assertRightDirectory();
    my($cvsfile) = AskAndPersistFile($main::filepaths{"sessionpath"});
    my($session) = Moz::MacCVS->new( $cvsfile );
    unless (defined($session)) { die "Error: Checkout aborted. Cannot create session file: $session" }

    my($checkout_start_time) = time();
    
    # activate MacCVS
    ActivateApplication('Mcvs');

    my($this_co);
    foreach $this_co (@$modules)
    {
        my($module, $revision, $date) = ($this_co->[0], $this_co->[1], $this_co->[2]);
        if ($date eq "") {
            $date = $pull_date;
        }
        CheckOutModule($session, $module, $revision, $date);
        # print "Checking out $module with ref $revision, date $date\n";
    }

    saveCheckoutTimestamp($checkout_start_time, $timestamp_file);
    TimeEnd($start_time, "Checkout");
}

#//--------------------------------------------------------------------------------------------------
#// ReadCheckoutModulesFile
#//--------------------------------------------------------------------------------------------------
sub ReadCheckoutModulesFile($$)
{
    my($modules_file, $co_list) = @_;

    my($checkout_file) = getScriptFolder().":".$modules_file;
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
    
        my(@cvs_co) = ["", "", ""];
        
        my($module, $revision, $date) = (0, 1, 2);
        
        if ($line =~ /\s*([^#,\s]+)\s*\,\s*([^#,\s]+)\s*\,\s*([^#]+)/)
        {
            @cvs_co[$module] = $1;
            @cvs_co[$revision] = $2;
            @cvs_co[$date] = $3;
        }
        elsif ($line =~ /\s*([^#,\s]+)\s*\,\s*([^#,\s]+)\s*(#.+)?/)
        {
            @cvs_co[$module] = $1;
            @cvs_co[$revision] = $2;
        }
        elsif ($line =~ /\s*([^#,\s]+)\s*\,\s*,\s*([^#,]+)/)       
        {
            @cvs_co[$module] = $1;
            @cvs_co[$date] = $2;
        }
        elsif ($line =~ /\s*([^#,\s]+)/)
        {
            @cvs_co[$module] = $1;
        }
        else
        {
            die "Error: unrecognized line '$line' in $modules_file\n";
        }
        
        # strip surrounding space from date
        @cvs_co[$date] =~ s/^\s*|\s*$//g;
        
        # print "Going to check out '@cvs_co[$module]', '@cvs_co[$revision]', '@cvs_co[$date]'\n";
        push(@$co_list, \@cvs_co);
    }

    close(CHECKOUT_FILE);
}

#//--------------------------------------------------------------------------------------------------
#// PullFromCVS
#//--------------------------------------------------------------------------------------------------
sub PullFromCVS($$)
{
    unless ( $main::build{pull} ) { return; }
    
    my($modules_file, $timestamp_file) = @_;

    StartBuildModule("pull");
    
    my(@cvs_co_list);
    ReadCheckoutModulesFile($modules_file, \@cvs_co_list);

    if ($main::FAST_UPDATE && $main::options{pull_by_date})
    {
        die "Error: you can't use FAST_UPDATE if you are pulling by date.\n";
    }

    my($did_fast_update) = $main::FAST_UPDATE && FastUpdate(\@cvs_co_list, $timestamp_file);
    if (!$did_fast_update)
    {
        my($pull_date) = "";
        if ($main::options{pull_by_date})
        {
            # acceptable CVS date formats are (in local time):
            # ISO8601 (e.g. "1972-09-24 20:05") and Internet (e.g. "24 Sep 1972 20:05").
            # Perl's localtime() string format also seems to work.
            $pull_date = localtime().""; # force string interp.
            print "Pulling by date $pull_date\n";
        }

        CheckoutModules(\@cvs_co_list, $pull_date, $timestamp_file);
    }

    EndBuildModule("pull");
}


#//--------------------------------------------------------------------------------------------------
#// RunBuild
#//--------------------------------------------------------------------------------------------------
sub RunBuild($$$$)
{
    my($do_pull, $do_build, $input_files, $build_prefs) = @_;

    InitBuildProgress($input_files->{"buildprogress"});
    
    # if we are pulling, we probably want to do a full build, so clear the build progress
    if ($do_pull) {
        ClearBuildProgress();    
    }
    
    # read local prefs, and the build progress file, and set flags to say what to build
    SetupBuildParams(\%main::build,
                     \%main::options,
                     \%main::optiondefines,
                     \%main::filepaths,
                     $input_files->{"buildflags"},
                     $build_prefs);
    
    # If we were told to pull, make sure we do, overriding prefs etc.
    if ($do_pull)
    {
        $main::build{"pull"} = 1;
    }
    
    # transfer this flag
    $CodeWarriorLib::CLOSE_PROJECTS_FIRST = $main::CLOSE_PROJECTS_FIRST;
    
    # setup the build log
    SetupBuildLog($main::filepaths{"buildlogfilepath"}, $main::USE_TIMESTAMPED_LOGS);
    StopForErrors();
    
    if ($main::LOG_TO_FILE) {
        RedirectOutputToFile($main::filepaths{"scriptlogfilepath"});
    }
        
    # run a pre-build check to see that the tools etc are in order
    DoPrebuildCheck();
    
    # do the pull
    PullFromCVS($input_files->{"checkoutdata"}, $input_files->{"checkouttime"});
    
    unless ($do_build) { return; }

    my($build_start) = TimeStart();

    # check the build environment
    ConfigureBuildSystem();

    # here we load and call methods in the build module indirectly.
    # we have to use indirection because the build module can be named
    # differently for different builds.
    chdir(dirname($0));     # change to the script dir
    my($build_module) = $input_files->{"buildmodule"};
    # load the build module
    require $build_module;
    {   # scope for no strict 'refs'
        no strict 'refs';
        
        my($package_name) = $build_module;
        $package_name =~ s/\.pm$//;
        
        chdir($main::MOZ_SRC);
        &{$package_name."::BuildDist"}();
        
        chdir($main::MOZ_SRC);
        &{$package_name."::BuildProjects"}();
    }
    
    # the build finished, so clear the build progress state
    ClearBuildProgress();
    
    TimeEnd($build_start, "Build");
    print "Build complete\n";
}

1;
