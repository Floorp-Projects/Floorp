
package MozBuildFlags;

require 5.004;
require Exporter;

# Package that attempts to read a file from the Preferences folder,
# and get build settings out of it

use strict;
use Exporter;

use Cwd;
use MozPrefs;

use vars qw(@ISA @EXPORT);

@ISA      = qw(Exporter);
@EXPORT   = qw(SetupBuildParams
               WriteBuildProgress
               ClearBuildProgress
               ReadBuildProgress);

my($script_dir) = cwd();



my(@pull_flags);
my(@build_flags);
my(@options_flags);
my(@filepath_flags);

my(%arrays_list) = (
  "pull_flags",     \@pull_flags,
  "build_flags",    \@build_flags,
  "options_flags",  \@options_flags,
  "filepath_flags", \@filepath_flags
);

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
    push(@{$flags_array}, @this_flag);
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
      if ($line =~ /^(\w+)\s*$/)
      {
        $cur_array = $1;
        next;  
      }
      elsif ($line =~ /^(\w+)\s+\"(.+)\"\s*$/)    # quoted option
      {
        my($flag) = $1;
        my($setting) = $2;
        
        appendArrayFlag($cur_array, $flag, $setting);
      }
      elsif ($line =~ /^(\w+)\s+([^\s]+)\s*$/)
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


#-------------------------------------------------------------------------------
# SetPullFlags
#-------------------------------------------------------------------------------
sub SetPullFlags($)
{
  my($pull) = @_;
  
  flagsArrayToHash(\@pull_flags, $pull);
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
  $optiondefines->{"mathml"}{"MOZ_MATHML"}		= 1;
  $optiondefines->{"svg"}{"MOZ_SVG"}		    	= 1;
}


#-------------------------------------------------------------------------------
# PropagateAllFlags
#-------------------------------------------------------------------------------
sub PropagateAllFlags($)
{
  my($build_array) = @_;
  
  # if "all" is set, set all the flags to 1
  unless ($build_array->[0][0] eq "all") { die "Error: 'all' must come first in the build array\n"; }
  
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
  my($progress_file);
  
  if ($main::DEBUG) {
    $progress_file = $script_dir.":¥Debug build Progress";
  } else {
    $progress_file = $script_dir.":¥Opt build Progress";
  }
  return $progress_file;
}


#//--------------------------------------------------------------------------------------------------
#// setBuildProgressStart
#//--------------------------------------------------------------------------------------------------
sub setBuildProgressStart($$)
{
  my($build_array, $name) = @_;

  my($setting) = 0;
  my($index);
  foreach $index (@$build_array)
  {
    $index->[1] = $setting;    

    if ($index->[0] eq $name) {
      $setting = 1;
    }    
  }

  if ($setting == 1) {
    print "Building from module after $name, as specified by build progress\n";
  } else {
    printf "Failed to find buildfrom setting '$name'\n";
  }
}


#//--------------------------------------------------------------------------------------------------
#// WriteBuildProgress
#//--------------------------------------------------------------------------------------------------
sub WriteBuildProgress($)
{
  my($module_built) = @_;

  my($progress_file) = _getBuildProgressFile();
  
  open(PROGRESS_FILE, ">>$progress_file") || die "Failed to open $progress_file\n";
  print(PROGRESS_FILE "$module_built\n");
  close(PROGRESS_FILE);
}


#//--------------------------------------------------------------------------------------------------
#// ClearBuildProgress
#//--------------------------------------------------------------------------------------------------
sub ClearBuildProgress()
{
  my($progress_file) = _getBuildProgressFile();
  unlink $progress_file;
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
# SetupBuildParams
#-------------------------------------------------------------------------------
sub SetupBuildParams($$$$$$$)
{
  my($pull, $build, $options, $optiondefines, $filepaths, $flags_file, $prefs_file) = @_;
  
  readFlagsFile($flags_file);
  
  # read the user pref file, that can change values in the array
  ReadMozUserPrefs($prefs_file, \@pull_flags, \@build_flags, \@options_flags, \@filepath_flags);

  ReadBuildProgress(\@build_flags);
  
  PropagateAllFlags(\@build_flags);
  PropagateAllFlags(\@pull_flags);
  
  SetPullFlags($pull);
  SetBuildFlags($build);
  SetBuildOptions($options);
  SetOptionDefines($optiondefines);
  SetFilepathFlags($filepaths);
  
  # printHash($build);
}


1;
