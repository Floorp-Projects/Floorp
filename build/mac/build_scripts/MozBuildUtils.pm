
package MozBuildUtils;

require 5.004;
require Exporter;

# Package that contains build util functions specific to the Mozilla build
# process.

use strict;
use Exporter;

use Cwd;

use Mac::Events;
use Mac::StandardFile;

use Moz;

use vars qw(@ISA @EXPORT);

@ISA      = qw(Exporter);
@EXPORT   = qw(GetBinDirectory
               BuildOneProject
               BuildIDLProject
               BuildFolderResourceAliases
               AskAndPersistFile
               DelayFor
               EmptyTree
               SetupBuildLog
               SetBuildNumber
               SetTimeBomb);


#--------------------------------------------------------------------------------------------------
# GetBinDirectory
#--------------------------------------------------------------------------------------------------
sub GetBinDirectory()
{
    if ($main::BIN_DIRECTORY eq "") { die "Dist directory not set\n"; }
    return $main::BIN_DIRECTORY;
}

#--------------------------------------------------------------------------------------------------
# AskAndPersistFile stores the information about the user pick inside
# the file $session_storage
#--------------------------------------------------------------------------------------------------
sub AskAndPersistFile($)
{
    my ($sessionStorage) = @_;
    my $cvsfile;

    if (( -e $sessionStorage) &&
        open( SESSIONFILE, $sessionStorage ))
    {
    # Read in the path if available
        $cvsfile = <SESSIONFILE>;
        chomp $cvsfile;
        close SESSIONFILE;
        if ( ! -e $cvsfile )
        {
            print STDERR "$cvsfile has disappeared\n";
            undef $cvsfile;
        }
    }
    unless (defined ($cvsfile))
    {
        # make sure that MacPerl is a front process
        ActivateApplication('McPL');
        MacPerl::Answer("Could not find your MacCVS session file. Please choose one", "OK");
        
        # prompt user for the file name, and store it
        my $macFile = StandardGetFile( 0, "McvD");  
        if ( $macFile->sfGood() )
        {
            $cvsfile = $macFile->sfFile();
            # save the choice if we can
            if ( open (SESSIONFILE, ">" . $sessionStorage))
            {
                printf SESSIONFILE $cvsfile, "\n";
                close SESSIONFILE;
            }
            else
            {
                print STDERR "Could not open storage file\n";
            }
        }
    }
    return $cvsfile;
}


#--------------------------------------------------------------------------------------------------
# BuildIDLProject
#
#--------------------------------------------------------------------------------------------------

sub BuildIDLProject($$)
{
    my ($project_path, $module_name) = @_;

    if ($main::CLOBBER_IDL_PROJECTS)
    {
        my($datafolder_path) = $project_path;
        $datafolder_path =~ s/\.mcp$/ Data:/;       # generate name of the project's data folder.
        print STDERR "Deleting IDL data folder: $datafolder_path\n";
        EmptyTree($datafolder_path);
    }

    BuildOneProject($project_path,  "headers", 0, 0, 0);
    BuildOneProject($project_path,  $module_name.".xpt", 1, 0, 1);
}


#//--------------------------------------------------------------------------------------------------
#// Build one project, and make the alias. Parameters
#// are project path, target name, make shlb alias (boolean), make xSYM alias (boolean)
#// 
#// Note that this routine assumes that the target name and the shared libary name
#// are the same.
#//--------------------------------------------------------------------------------------------------

sub BuildOneProject($$$$$)
{
    my ($project_path, $target_name, $alias_shlb, $alias_xSYM, $component) = @_;

    unless ($project_path =~ m/^$main::BUILD_ROOT.+/) { return; }
    
    # $D becomes a suffix to target names for selecting either the debug or non-debug target of a project
    my($D) = $main::DEBUG ? "Debug" : "";
    my($dist_dir) = GetBinDirectory();
    
    # Put libraries in "Essential Files" folder, Components in "Components" folder
    my($component_dir) = $component ? "Components:" : "Essential Files:";

    my($project_dir) = $project_path;
    $project_dir =~ s/:[^:]+$/:/;           # chop off leaf name

    if ($main::CLOBBER_LIBS)
    {
        unlink "$project_dir$target_name";              # it's OK if these fail
        unlink "$project_dir$target_name.xSYM";
    }
    
    BuildProject($project_path, $target_name);
    
    $alias_shlb ? MakeAlias("$project_dir$target_name", "$dist_dir$component_dir") : 0;
    $alias_xSYM ? MakeAlias("$project_dir$target_name.xSYM", "$dist_dir$component_dir") : 0;
}


#//--------------------------------------------------------------------------------------------------
#// Make resource aliases for one directory
#//--------------------------------------------------------------------------------------------------

sub BuildFolderResourceAliases($$)
{
    my($src_dir, $dest_dir) = @_;
        
    # get a list of all the resource files
    opendir(SRCDIR, $src_dir) || die("can't open $src_dir");
    my(@resource_files) = readdir(SRCDIR);
    closedir(SRCDIR);
    
    # make aliases for each one into the dest directory
    print("Placing aliases to all files from $src_dir in $dest_dir\n");
    for ( @resource_files )
    {
        next if $_ eq "CVS";
        #print("    Doing $_\n");
        if (-l $src_dir.$_)
        {
            print("     $_ is an alias\n");
            next;
        }
        my($file_name) = $src_dir . $_; 
        MakeAlias($file_name, $dest_dir);
    }
}


#//--------------------------------------------------------------------------------------------------
#// DelayFor
#//
#// Delay for the given number of seconds, allowing the script to be cancelled
#//--------------------------------------------------------------------------------------------------

sub DelayFor($)
{
  my($delay_secs) = @_;
  
  STDOUT->autoflush(1);
  
  my($end_time) = time() + $delay_secs;
    
  my($last_time);
  my($cur_time) = time();
  
  while ($cur_time < $end_time)
  {
    $cur_time = time();
    if ($cur_time > $last_time)
    {
      print ".";
      $last_time = $cur_time;
    }
    
    WaitNextEvent();
  }

  STDOUT->autoflush(0);
}


#//--------------------------------------------------------------------------------------------------
#// Remove all files from a tree, leaving directories intact (except "CVS").
#//--------------------------------------------------------------------------------------------------

sub EmptyTree($)
{
    my ($root) = @_; 
    #print "EmptyTree($root)\n";
    opendir(DIR, $root);
    my $sub;
    foreach $sub (readdir(DIR))
    {
        my $fullpathname = $root.$sub; # -f, -d only work on full paths

        # Don't call empty tree for the alias of a directory.
        # -d returns true for the alias of a directory, false for a broken alias)

        if (-d $fullpathname)
        {
            if (-l $fullpathname)   # delete aliases
            {
                unlink $fullpathname;
                next;
            }
            EmptyTree($fullpathname.":");
            if ($sub eq "CVS")
            {
                #print "rmdir $fullpathname\n";
                rmdir $fullpathname;
            }
        }
        else
        {
            unless (unlink $fullpathname) { die "Failed to delete $fullpathname\n"; }
        }
    }
    closedir(DIR);
}


#-----------------------------------------------
# SetupBuildLog
#-----------------------------------------------
sub SetupBuildLog($)
{
    my($timestamped_log) = @_;
    
    if ($timestamped_log)
    {
        #Use time-stamped names so that you don't clobber your previous log file!
        my $now = localtime();
        while ($now =~ s@:@.@) {} # replace all colons by periods
        my $logdir = ":Build Logs:";
        if (! -d $logdir)
        {
            mkdir $logdir, 0777 || die "Couldn't create directory $logdir";
        }
        OpenErrorLog("$logdir$now");
    }
    else
    {
        OpenErrorLog("Mozilla build log");
    }
}

#-----------------------------------------------
# SetBuildNumber
#-----------------------------------------------
sub SetBuildNumber($$)
{
    my($build_num_file, $files_to_touch) = @_;

    # Make sure we add the config dir to search, to pick up mozBDate.pm
    # Need to do this dynamically, because this module can be used before
    # mozilla/config has been checked out.
    
    my ($inc_path) = $0;        # $0 is the path to the parent script
    $inc_path =~ s/:build:mac:build_scripts:.+$/:config/;
    push(@INC, $inc_path);

    require mozBDate;
    
    mozBDate::UpdateBuildNumber($build_num_file, $main::MOZILLA_OFFICIAL);
   
    my($file);
    foreach $file (@$files_to_touch)
    {
        print "Writing build number to $file from ${file}.in\n";
        mozBDate::SubstituteBuildNumber($file, $build_num_file, "${file}.in");
    }
}

#-----------------------------------------------
# SetBuildNumber
#-----------------------------------------------
sub SetTimeBomb($$)		
{
  my ($warn_days, $bomb_days) = @_;
  
  system("perl :mozilla:config:mac-set-timebomb.pl $warn_days $bomb_days");
}


1;

