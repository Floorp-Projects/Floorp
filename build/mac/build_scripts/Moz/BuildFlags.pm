
package Moz::BuildFlags;

require 5.004;
require Exporter;

# Package that attempts to read a file from the Preferences folder,
# and get build settings out of it

use strict;
use Exporter;

use Cwd;
use File::Basename;

use Moz::Moz;
use Moz::Prefs;

use vars qw(@ISA @EXPORT);

@ISA      = qw(Exporter);
@EXPORT   = qw(
                SetupBuildParams
                InitBuildProgress
                WriteBuildProgress
                ClearBuildProgress
                ReadBuildProgress
              );


my(@build_flags);
my(@options_flags);
my(@filepath_flags);

my(%arrays_list) = (
  "build_flags",    \@build_flags,
  "options_flags",  \@options_flags,
  "filepath_flags", \@filepath_flags
);

my($progress_file) = "¥ÊBuild progress";

#-------------------------------------------------------------------------------
# appendArrayFlag
# 
# Set a flag in the array
#-------------------------------------------------------------------------------
sub appendArrayFlag($$$)
{
  my($array_name, $setting, $value) = @_;
  
  my(@this_flag) = [$setting, $value];
  my($flags_array) = $arrays_list{$array_name};
  if ($flags_array)
  {
    push(@{$flags_array}, @this_flag) || die "Failed to append\n";
  }
  else
  {
    die "Error: unknown build flags array $array_name\n";
  }
}

#-------------------------------------------------------------------------------
# readFlagsFile
# 
# Read the file of build flags from disk. File path is relative to the
# script directory.
#-------------------------------------------------------------------------------
sub readFlagsFile($)
{
  my($flags_file) = @_;

  my($file_path) = $0;
  $file_path =~ s/[^:]+$/$flags_file/;
  
  print "Reading build flags from '$file_path'\n";

  local(*FLAGS_FILE);
  open(FLAGS_FILE, "< $file_path") || die "Error: failed to open flags file $file_path\n";

  my($cur_array) = "";
  
  while(<FLAGS_FILE>)
  {
      my($line) = $_;
      chomp($line);
      
      # ignore comments and empty lines  
      if ($line =~ /^\#/ || $line =~ /^\s*$/) {
        next;
      }

      # 1-word line, probably array name
      if ($line =~ /^([^#\s]+)\s*$/)
      {
        $cur_array = $1;
        next;  
      }
      elsif ($line =~ /^([^#\s]+)\s+\"(.+)\"(\s+#.+)?$/)    # quoted option, possible comment
      {
        my($flag) = $1;
        my($setting) = $2;
        
        appendArrayFlag($cur_array, $flag, $setting);
      }
      elsif ($line =~ /^([^#\s]+)\s+([^#\s]+)(\s+#.+)?$/)   # two-word line, possible comment
      {
        my($flag) = $1;
        my($setting) = $2;
        
        appendArrayFlag($cur_array, $flag, $setting);
      }
      else
      {
        die "Error: unknown build flag at '$line'\n";
      }
  
  }
  
  close(FLAGS_FILE);
}



#-------------------------------------------------------------------------------
# flagsArrayToHash
# 
# Utility routine to migrate flag from a 2D array to a hash, where
# item[n][0] is the hash entry name, and item[n][1] is the hash entry value.
#-------------------------------------------------------------------------------

sub flagsArrayToHash($$)
{
  my($src_array, $dest_hash) = @_;
  
  my($item);
  foreach $item (@$src_array)
  {
    $dest_hash->{$item->[0]} = $item->[1];
  }
}

#-----------------------------------------------
# printHash
#
# Utility routine to print a hash
#-----------------------------------------------
sub printHash($)
{
  my($hash_ref) = @_;
 
  print "Printing hash:\n";
  
  my($key, $value);
  
  while (($key, $value) = each (%$hash_ref))
  {
    print "  $key $value\n";
  }
}


#-----------------------------------------------
# printBuildArray
#
# Utility routine to print a 2D array
#-----------------------------------------------
sub printBuildArray($)
{
  my($build_array) = @_;
  
  my($entry);
  foreach $entry (@$build_array)
  {  
    print "$entry->[0] = $entry->[1]\n";
  }

}

#-------------------------------------------------------------------------------
# SetBuildFlags
#-------------------------------------------------------------------------------
sub SetBuildFlags($)
{
  my($build) = @_;

  flagsArrayToHash(\@build_flags, $build);
}

#-------------------------------------------------------------------------------
# SetBuildOptions
#-------------------------------------------------------------------------------
sub SetBuildOptions($)
{
  my($options) = @_;

  flagsArrayToHash(\@options_flags, $options);
}

#-------------------------------------------------------------------------------
# SetFilepathFlags
#-------------------------------------------------------------------------------
sub SetFilepathFlags($)
{
  my($filepath) = @_;

  flagsArrayToHash(\@filepath_flags, $filepath);
}

#-------------------------------------------------------------------------------
# SetOptionDefines
#-------------------------------------------------------------------------------
sub SetOptionDefines($)
{
  my($optiondefines) = @_;

  # These should remain unchanged
  $optiondefines->{"mathml"}{"MOZ_MATHML"}    = 1;
  $optiondefines->{"svg"}{"MOZ_SVG"}          = 1;
}


#-------------------------------------------------------------------------------
# PropagateAllFlags
#-------------------------------------------------------------------------------
sub PropagateAllFlags($)
{
  my($build_array) = @_;
  
  # if "all" is set, set all the flags to 1
  unless ($build_array->[0][0] eq "all") { die "Error: 'all' must come first in the flags array\n"; }
  
  if ($build_array->[0][1] == 1)
  {
    my($index);
    foreach $index (@$build_array)
    {
      $index->[1] = 1;
    }
  }
}


#//--------------------------------------------------------------------------------------------------
#// _getBuildProgressFile
#//--------------------------------------------------------------------------------------------------
sub _getBuildProgressFile()
{
  return $progress_file;
}


#//--------------------------------------------------------------------------------------------------
#// setBuildProgressStart
#// 
#// This automagically sets $build{"all"} to 0
#//--------------------------------------------------------------------------------------------------
sub setBuildProgressStart($$)
{
  my($build_array, $name) = @_;

  my($index);
  foreach $index (@$build_array)
  {
    $index->[1] = 0;
    if ($index->[0] eq $name) {
      last;
    }    
  }

  print "Building from module after $name, as specified by build progress\n";
}

#//--------------------------------------------------------------------------------------------------
#// InitBuildProgress
#//--------------------------------------------------------------------------------------------------
sub InitBuildProgress($)
{
  my($prog_file) = @_;
  if ($prog_file ne "") {
    $progress_file = full_path_to($prog_file);
    print "Writing build progress to $progress_file\n";
  }
}

#//--------------------------------------------------------------------------------------------------
#// WriteBuildProgress
#//--------------------------------------------------------------------------------------------------
sub WriteBuildProgress($)
{
  my($module_built) = @_;

  my($progress_file) = _getBuildProgressFile();
  
  if ($progress_file ne "")
  {
    open(PROGRESS_FILE, ">>$progress_file") || die "Failed to open $progress_file\n";
    print(PROGRESS_FILE "$module_built\n");
    close(PROGRESS_FILE);
  }
}


#//--------------------------------------------------------------------------------------------------
#// ClearBuildProgress
#//--------------------------------------------------------------------------------------------------
sub ClearBuildProgress()
{
  my($progress_file) = _getBuildProgressFile();
  if ($progress_file ne "") {
    unlink $progress_file;
  }
}

#//--------------------------------------------------------------------------------------------------
#// WipeBuildProgress
#//--------------------------------------------------------------------------------------------------
sub WipeBuildProgress()
{
  print "Ignoring build progress\n";
  ClearBuildProgress();
  $progress_file = "";
}

#//--------------------------------------------------------------------------------------------------
#// ReadBuildProgress
#//--------------------------------------------------------------------------------------------------
sub ReadBuildProgress($)
{
  my($build_array) = @_;
  my($progress_file) = _getBuildProgressFile();

  my($last_module);
  
  if (open(PROGRESS_FILE, "< $progress_file"))
  {
    print "Getting build progress from $progress_file\n";
    
    while (<PROGRESS_FILE>)
    {
      my($line) = $_;
      chomp($line);
      $last_module = $line;
    }
    
    close(PROGRESS_FILE);
  }
  
  if ($last_module)
  {
    setBuildProgressStart($build_array, $last_module);
  }
}

#-------------------------------------------------------------------------------
# clearOldBuildSettings
#-------------------------------------------------------------------------------
sub clearOldBuildSettings($$$$)
{
  my($build, $options, $optiondefines, $filepaths) = @_;

  # empty the arrays in case we're being called twice
  @build_flags = ();
  @options_flags = ();
  @filepath_flags = ();
  
  # and empty the hashes
  %$build = ();
  %$options = ();
  %$optiondefines = ();
  %$filepaths = ();
}

#-------------------------------------------------------------------------------
# SetupBuildParams
#-------------------------------------------------------------------------------
sub SetupBuildParams($$$$$$)
{
  my($build, $options, $optiondefines, $filepaths, $flags_file, $prefs_file) = @_;
  
  # Empty the hashes and arrays, to wipe out any stale data.
  # Needed because these structures persist across two build scripts
  # called using 'do' from a parent script.
  clearOldBuildSettings($build, $options, $optiondefines, $filepaths);
  
  # Read from the flags file, which sets up the various arrays
  readFlagsFile($flags_file);
  
  # If 'all' is set in the build array, propagate that to all entries
  PropagateAllFlags(\@build_flags);
  
  # read the user pref file, that can change values in the array
  ReadMozUserPrefs($prefs_file, \@build_flags, \@options_flags, \@filepath_flags);

  # If build progress exists, this clears flags in the array up to a certain point
  if ($main::USE_BUILD_PROGRESS) {
    ReadBuildProgress(\@build_flags);
  } else {
    WipeBuildProgress();
  }
  
#  printBuildArray(\@build_flags);
#  printBuildArray(\@options_flags);
  
  SetBuildFlags($build);
  SetBuildOptions($options);
  SetOptionDefines($optiondefines);
  SetFilepathFlags($filepaths);
  
#  printHash($build);
#  printHash($options);
}


1;
