#!perl -w
# 
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
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-1999
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Sean Su <ssu@netscape.com>
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

use Cwd;
return(1);

sub InstallChrome()
{
  # inOsType     - type of OS being run under
  # inType       - type of reg file to create (jar or resource)
  # inTargetPath - path to where the bin\chrome dir
  ($inOsType, $inType, $inTargetPath) = @_;
  my($mOutFilenameTmp) = "installed-chrome_tmp.txt";
  my($mFinalFilename)  = "installed-chrome.txt";

  if($inOsType =~ /win32/i)
  {
    $gPathDelimiter="\\";
  }
  elsif($inOsType =~ /mac/i)
  {
    $gPathDelimiter=":";
  }
  elsif($inOsType =~ /unix/i)
  {
    $gPathDelimiter="/";
  }

  if($inOsType =~ /win32/i)
  {
    # Convert all '/' to '\\' or else win32 will have problems
    $inTargetPath =~ s/\//\\/g;
  }

  # Make sure $inTargetPath exists
  if(!(-e "$inTargetPath"))
  {
    return(1);
  }

  open(fpOutFileTmp, ">$inTargetPath$gPathDelimiter$mOutFilenameTmp") || die "\nCould not open $inTargetPath$gPathDelimiter$mOutFilenameTmp: $!\n";

  CreateChromeTextFile($inType, $inTargetPath, "locales",  "loc");
  CreateChromeTextFile($inType, $inTargetPath, "packages", "pkg");
  CreateChromeTextFile($inType, $inTargetPath, "skins",    "skn");

  if(-e "$inTargetPath$gPathDelimiter$mFinalFilename")
  {
    open(fpInFile, "$inTargetPath$gPathDelimiter$mFinalFilename") || die "\nCould not open $inTargetPath$gPathDelimiter$mFinalFilename: $!\n";
    while($line = <fpInFile>)
    {
      print fpOutFileTmp "$line";
    }

    close(fpInFile);
    unlink "$inTargetPath$gPathDelimiter$mFinalFilename";
  }
  close(fpOutFileTmp);

  if(!rename("$inTargetPath$gPathDelimiter$mOutFilenameTmp", "$inTargetPath$gPathDelimiter$mFinalFilename"))
  {
    die "\n Error $!: rename (\"$inTargetPath$gPathDelimiter$mOutFilenameTmp\", \"$inTargetPath$gPathDelimiter$mFinalFilename\")\n";
  }
  return(0);
}

sub CreateChromeTextFile()
{
  my($inType, $inTargetPath, $inChromeDir, $inExt) = @_;

  @dlChromeDir = <$inTargetPath\\$inChromeDir\\*>;
  foreach $dir (@dlChromeDir)
  {
    if($inOsType =~ /win32/i)
    {
      # Convert all '/' to '\\' or else win32 will have problems
      $dir =~ s/\//\\/g;
    }

    # Get the leaf dir name from full path
    if($inOsType =~ /win32/i)
    {
      @dirItem = split(/\\/, $dir);
    }
    elsif($inOsType =~ /mac/i)
    {
      @dirItem = split(/:/, $dir);
    }
    elsif($inOsType =~ /unix/i)
    {
      @dirItem = split(/\//, $dir);
    }
    $dirName = $dirItem[$#dirItem];

    # Make sure the path is valid
    if(-d "$dir")
    {
      if($inType =~ /jar/i)
      {
        if(CheckDir("content", "$dir"))
        {
          print fpOutFileTmp "content,install,url,jar:resource:/chrome/$dirName.$inExt!/\n";
        }
        if(CheckDir("locale", "$dir"))
        {
          print fpOutFileTmp "locale,install,url,jar:resource:/chrome/$dirName.$inExt!/\n";
        }
        if(CheckDir("skin", "$dir"))
        {
          print fpOutFileTmp "skin,install,url,jar:resource:/chrome/$dirName.$inExt!/\n";
        }
      }
      else
      {
        if(CheckDir("content", "$dir"))
        {
          print fpOutFileTmp "content,install,url,resource:/chrome/$inChromeDir/$dirName/\n";
        }
        if(CheckDir("locale", "$dir"))
        {
          print fpOutFileTmp "locale,install,url,resource:/chrome/$inChromeDir/$dirName/\n";
        }
        if(CheckDir("skin", "$dir"))
        {
          print fpOutFileTmp "skin,install,url,resource:/chrome/$inChromeDir/$dirName/\n";
        }
      }
    }
  }

  return(0);
}

sub CheckDir()
{
  my($dirType, $inPath) = @_;

  @dlDirType = <$inPath$gPathDelimiter*>;
  foreach $dir (@dlDirType)
  {
    if($inOsType =~ /win32/i)
    {
      # Convert all '/' to '\\' or else win32 will have problems
      $dir =~ s/\//\\/g;
    }

    if(-d "$dir$gPathDelimiter$dirType")
    {
      return(1);
    }
  }
  return(0);
}

