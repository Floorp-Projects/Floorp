#!perl -w
package Moz::BuildCore;

require 5.004;
require Exporter;

use strict;
use vars qw( @ISA @EXPORT );

# perl includes
use Cwd;
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
#// FastUpdate
#// 
#// Use Bonsai url data to update only those dirs which have new files
#// 
#//--------------------------------------------------------------------------------------------------
sub FastUpdate($)
{
    my($num_hours) = @_;

    my($the_module) = "SeaMonkeyAll";
    my($the_branch) = "HEAD";
    my($search_type) = "hours";
    my($min_date) = "";
    my($max_date) = "";
    my($url) = "http://bonsai.mozilla.org/cvsquery.cgi?treeid=default&module=${the_module}&branch=${the_branch}&branchtype=match&dir=&file=&filetype=match&who=&whotype=match&sortby=Date&hours=${num_hours}&date=${search_type}&mindate=${min_date}&maxdate=${max_date}&cvsroot=%2Fcvsroot";

    my(@files) = &get_files_from_content(&get_url_contents($url));

	my(@cvs_co_list);

	my($co_file);
	foreach $co_file (@files)
	{
	  my(@cvs_co) = ["", "", ""];
	  @cvs_co[0] = $co_file;
	  push(@cvs_co_list, \@cvs_co);
	}

	CheckoutModules(\@cvs_co_list);
}


#//--------------------------------------------------------------------------------------------------
#// Checkout
#//--------------------------------------------------------------------------------------------------
sub CheckoutModules($)
{
    my($modules) = @_;          # list of modules to check out

    my($start_time) = TimeStart();

#    assertRightDirectory();
    my($cvsfile) = AskAndPersistFile($main::filepaths{"sessionpath"});
    my($session) = Moz::MacCVS->new( $cvsfile );
    unless (defined($session)) { die "Error: Checkout aborted. Cannot create session file: $session" }

    # activate MacCVS
    ActivateApplication('Mcvs');

    my($this_co);
    foreach $this_co (@$modules)
    {
        my($module, $revision, $date) = ($this_co->[0], $this_co->[1], $this_co->[2]);        
        CheckOutModule($session, $module, $revision, $date);
        # print "Checking out $module with ref $revision, date $date\n";
    }

    TimeEnd($start_time, "Checkout");
}

#//--------------------------------------------------------------------------------------------------
#// Checkout
#//--------------------------------------------------------------------------------------------------
sub Checkout($)
{
    my($checkout_list) = @_;
    
    unless ( $main::build{pull} ) { return; }

    StartBuildModule("pull");

    my(@cvs_co_list);
    
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
            die "Error: unrecognized line '$line' in $checkout_list\n";
        }
        
        # strip surrounding space from date
        @cvs_co[$date] =~ s/^\s*|\s*$//g;
        
        # print "Going to check out '@cvs_co[$module]', '@cvs_co[$revision]', '@cvs_co[$date]'\n";
        push(@cvs_co_list, \@cvs_co);
    }

    close(CHECKOUT_FILE);

    CheckoutModules(\@cvs_co_list);

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
    
    if ($main::FAST_UPDATE)
    {
        my($hours) = 8;     # update files checked in during last 8 hours
        FastUpdate($hours);
    } else {
        Checkout($input_files->{"checkoutdata"});
    }
    
    unless ($do_build) { return; }

    my($build_start) = TimeStart();

    # create generated headers
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
