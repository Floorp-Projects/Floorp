#!/usr/bin/perl

#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#   Simon Fraser <sfraser@netscape.com>
#

package Moz::ProjectXML;

require 5.004;
require Exporter;

use strict;
use Exporter;

use Cwd;
use XML::DOM;

use vars qw(@ISA @EXPORT);

@ISA      = qw(Exporter);
@EXPORT   = qw(
                ParseXMLDocument
                DisposeXMLDocument
                WriteXMLDocument
                CleanupPro5XML
                GetTargetsList
                CloneTarget
                SetAsSharedLibraryTarget
                SetAsStaticLibraryTarget
                AddTarget
                RemoveTarget
                GetTargetSetting
                SetTargetSetting
                getChildElementTextContents
              );


#--------------------------------------------------------------------------------------------------
# A module for reading, manipulating, and writing XML-format CodeWarrior project files.
# 
# Sample usage:
# 
# use ProjectXML;
# 
# my $doc = ProjectXML::ParseXMLDocument("Test.mcp.xml");
# ProjectXML::CloneTarget($doc, "Test.shlb", "Test.lib");
# ProjectXML::SetAsStaticLibraryTarget($doc, "Test.lib", "TestOutput.lib");
# ProjectXML::WriteXMLDocument($doc, "Test_out.xml");
# ProjectXML::DisposeXMLDocument($doc);
# 
#--------------------------------------------------------------------------------------------------


#//--------------------------------------------------------------------------------------------------
#// ParseXMLDocument
#// Note that the caller must call DisposeXMLDocument on the returned doc
#//--------------------------------------------------------------------------------------------------
sub ParseXMLDocument($)
{
  my($doc_path) = @_;

  my $parser = new XML::DOM::Parser(ErrorContext => 2);
  my $doc = $parser->parsefile($doc_path);

  return $doc;
}

#//--------------------------------------------------------------------------------------------------
#// DisposeXMLDocument
#// Needed to avoid memory leaks - cleanup circular references for garbage collection
#//--------------------------------------------------------------------------------------------------
sub DisposeXMLDocument($)
{
  my($doc) = @_;
  
  $doc->dispose();
}


#//--------------------------------------------------------------------------------------------------
#// WriteXMLDocument
#//--------------------------------------------------------------------------------------------------

sub _pro5_tag_compression($$)
{
   return 1;    # Pro 5 is broken and can't import XML with <foo/> style tags
}

sub _pro6plus_tag_compression($$)
{
   return 0;    # Pro 6 can deal with empty XML tags like <foo/>
}

sub WriteXMLDocument($$$)
{
  my($doc, $file_path, $ide_version) = @_;

  if ($ide_version eq "4.0")
  {
    XML::DOM::setTagCompression(\&_pro5_tag_compression);
  }
  else
  {
    XML::DOM::setTagCompression(\&_pro6plus_tag_compression);
  }

  $doc->printToFile($file_path);
}


#//--------------------------------------------------------------------------------------------------
#// CleanupPro5XML
#// XML Projects exported by Pro 5 contain garbage data under the MWMerge_MacOS_skipResources
#// setting. This routine cleans this up, saving the result to a new file
#//--------------------------------------------------------------------------------------------------
sub CleanupPro5XML($$)
{
  my($xml_path, $out_path) = @_;
  
  local(*XML_FILE);
  open(XML_FILE, "< $xml_path") || die "Error: failed to open file $xml_path\n";
  
  local(*CLEANED_FILE);
  open(CLEANED_FILE, "> $out_path") || die "Error: failed to open file $out_path for writing\n";
  
  my $in_skip_resources_settings = 0;
  
  while(<XML_FILE>)
  {
      my($line) = $_;

      if ($line =~ /^<\?codewarrior/)    # is processing inst line
      {
        my $test_line = $line;
        chomp($test_line);
  
        my $out_line = $test_line;
        if ($test_line =~ /^<\?codewarrior\s+exportversion=\"(.+)\"\s+ideversion=\"(.+)\"\s*\?>$/)
        {
          my $export_version = $1;
          my $ide_version    = $2;

          $ide_version = "4.0_mozilla";   # pseudo IDE version so we know we touched it
          $out_line = "<?codewarrior exportversion=\"".$export_version."\" ideversion=\"".$ide_version."\"?>";
        }

        print CLEANED_FILE "$out_line\n";
        next;
      }
      
      if ($line =~ /MWMerge_MacOS_skipResources/)
      {
        $in_skip_resources_settings = 1;
        print CLEANED_FILE "$line";
      }
      elsif($in_skip_resources_settings && $line =~ /<!-- Settings for/)
      {
        # leaving bad settings lines. Write closing tag
        print CLEANED_FILE "                    <!-- Corrupted setting entries removed by script -->\n";
        print CLEANED_FILE "                </SETTING>\n\n";
        
        print CLEANED_FILE "$line";

        $in_skip_resources_settings = 0;
      }
      elsif (!$in_skip_resources_settings)
      {      
        print CLEANED_FILE "$line";
      }
  }
  
  close(XML_FILE);
  close(CLEANED_FILE);

}

#--------------------------------------------------------------------------------------------------
# SniffProjectXMLIDEVersion
#
#--------------------------------------------------------------------------------------------------
sub SniffProjectXMLIDEVersion($)
{
  my($xml_path) = @_;
  
  my $found_version = "";
  
  local(*XML_FILE);
  open(XML_FILE, "< $xml_path") || die "Error: failed to open file $xml_path\n";
  
  while(<XML_FILE>)
  {
      my($line) = $_;
      chomp($line);
      
      if ($line =~ /^<\?codewarrior/)    # is processing inst line
      {
        unless ($line =~ /^<\?codewarrior\s+exportversion=\"(.+)\"\s+ideversion=\"(.+)\"\s*\?>$/)
        {
          die "Error: Failed to find ideversion in $xml_path in line $line\n";
        }
        
        my $export_version = $1;
        my $ide_version    = $2;

        $found_version = $ide_version;         
        last;
      }
  }
  
  close(XML_FILE);

  return $found_version;
}

#//--------------------------------------------------------------------------------------------------
#// GetTargetsList
#// Returns an array of target names
#//--------------------------------------------------------------------------------------------------
sub GetTargetsList($)
{
  my($doc) = @_;

  my $nodes = $doc->getElementsByTagName("TARGET");
  my $n = $nodes->getLength;
  
  my @target_names;
  
  for (my $i = 0; $i < $n; $i++)
  {
    my ($node) = $nodes->item($i);   
   
    my($target_name) = getChildElementTextContents($node, "NAME");
    push(@target_names, $target_name);
  }

  return @target_names;
}


#//--------------------------------------------------------------------------------------------------
#// CloneTarget
#// Clone the named target, renaming it to 'new_name'
#//--------------------------------------------------------------------------------------------------
sub CloneTarget($$$)
{
  my($doc, $target_name, $new_name) = @_;

  my $target_node   = getTargetNode($doc, $target_name);

  # clone here
  my $target_clone  = $target_node->cloneNode(1);   # deep clone
  
  # -- munge target settings --

  # set the target name field
  setChildElementTextContents($doc, $target_clone, "NAME", $new_name);
  
  # set the targetname pref
  setTargetNodeSetting($doc, $target_clone, "Targetname", $new_name);

  # -- insert new target subtree --
  
  my $target_list   = $target_node->getParentNode();
  $target_list->appendChild($target_clone);
  
  # -- now add to targetorder --
  my (@target_order_nodes) = getChildOfDocument($doc, "TARGETORDER");
  
  my $target_order = @target_order_nodes[0];
  
  my $new_order     = $doc->createElement("ORDEREDTARGET");
  my $order_name    = $doc->createElement("NAME");
  
  $new_order->appendChild($order_name);

  setChildElementTextContents($doc, $new_order, "NAME", $new_name);
  $target_order->appendChild($new_order);
}


#//--------------------------------------------------------------------------------------------------
#// SetAsSharedLibraryTarget
#// 
#//--------------------------------------------------------------------------------------------------
sub SetAsSharedLibraryTarget($$$)
{
  my($doc, $target_name, $output_name) = @_;

  my $target_node = getTargetNode($doc, $target_name);

  setTargetNodeSetting($doc, $target_node, "MWProject_PPC_type", "SharedLibrary");
  setTargetNodeSetting($doc, $target_node, "MWProject_PPC_filetype", "1936223330"); #'shlb'
  setTargetNodeSetting($doc, $target_node, "MWProject_PPC_outfile", $output_name);
}


#//--------------------------------------------------------------------------------------------------
#// AddFileToTarget
#// 
#// Add a file to the specified target(s).
#// 
#//--------------------------------------------------------------------------------------------------
sub AddFileToTarget($$$)
{
  my($doc, $target_list, $file_name) = @_;

  # the file must be added in 3 places:
  # 1. in <TARGET><FILELIST><FILE> (with linkage flags if necessary)
  # 2. in <TARGET><LINKORDER><FILEREF>
  # 3. in <GROUPLIST><GROUP><FILEREF>
  die "Write me\n";
}

#//--------------------------------------------------------------------------------------------------
#// RemoveFileFromTarget
#// 
#// Remove a file from the specified target, removing it from the entire project
#// if no other targets reference it.
#// 
#//--------------------------------------------------------------------------------------------------
sub RemoveFileFromTarget($$$)
{
  my($doc, $target_node, $file_name) = @_;
  
  # the file must be removed in 3 places:
  # 1. in <TARGET><FILELIST><FILE>
  # 2. in <TARGET><LINKORDER><FILEREF>
  # 3. in <GROUPLIST><GROUP><FILEREF>
  
  # first, remove from <FILELIST>
  my $filelist_node = getFirstChildElement($target_node, "FILELIST");
  unless ($filelist_node) { die "Error: failed to find FILELIST node\n"; }
  
  my $file_node = getChildNodeByGrandchildContents($doc, $filelist_node, "FILE", "PATH", $file_name);
  unless ($file_node) { return; }

  $filelist_node->removeChild($file_node);
  
  # next, remove from <LINKORDER>
  my $linkorder_node = getFirstChildElement($target_node, "LINKORDER");
  unless ($linkorder_node) { die "Error: failed to find LINKORDER node\n"; }
  
  my $fileref_node = getChildNodeByGrandchildContents($doc, $linkorder_node, "FILEREF", "PATH", $file_name);
  unless ($fileref_node) { die "Error: link order node for file $file_name not found\n"; }

  $linkorder_node->removeChild($fileref_node);
  
  # last, remove from <GROUPLIST>
  # <GROUPLIST> is cross-target, so we have to be careful here.
  my $grouplist_node = getChildOfDocument($doc, "GROUPLIST");
  unless ($grouplist_node) { die "Error: failed to find GROUPLIST node\n"; }

  # if the file isn't in any other targets, remove it from the groups
  if (!GetFileInUse($doc, $file_name))
  {
    print "File $file_name is in no other targest. Removing from project\n";

    my @group_nodes;
    getChildElementsOfType($doc, $grouplist_node, "GROUP", \@group_nodes);
    my $group_node;
    foreach $group_node (@group_nodes)
    {
      my @fileref_nodes;
      getChildElementsOfType($doc, $group_node, "FILEREF", \@fileref_nodes);
  
      my $fileref_node;
      foreach $fileref_node (@fileref_nodes)
      {
        my $path_name = getChildElementTextContents($fileref_node, "PATH");
        if ($path_name eq $file_name)
        {
          print "Removing $file_name from project group list\n";
          $group_node->removeChild($fileref_node);
          last;
        }
      }
      
      # can a file appear in more than one group?
    }
  }
}

#//--------------------------------------------------------------------------------------------------
#// SetAsStaticLibraryTarget
#// 
#//--------------------------------------------------------------------------------------------------
sub SetAsStaticLibraryTarget($$$)
{
  my($doc, $target_name, $output_name) = @_;

  my $target_node = getTargetNode($doc, $target_name);

  setTargetNodeSetting($doc, $target_node, "MWProject_PPC_type", "Library");
  setTargetNodeSetting($doc, $target_node, "MWProject_PPC_filetype", "1061109567"); #'????'
  setTargetNodeSetting($doc, $target_node, "MWProject_PPC_outfile", $output_name);

  # static targets don't need any library linkage, so we can remove linkage
  # with all .shlb and .Lib files.
  
  my(@obsolete_files) = ("NSStdLibStubs", "InterfacesStubs", "InterfaceLib", "InternetConfigLib");

  print " Removing libraries etc. from target\n";

  # get all files in target
  my @target_files = GetTargetFilesList($doc, $target_name);
  my $target_file;
  foreach $target_file (@target_files)
  {
    if ($target_file =~ /(\.shlb|\.lib|\.Lib|\.o|\.exp)$/)
    {
      RemoveFileFromTarget($doc, $target_node, $target_file);
    }
  }
  
  print " Removing stub libraries from target\n";
  
  # then remove files with known names
  my $obs_file;
  foreach $obs_file (@obsolete_files)
  {
    RemoveFileFromTarget($doc, $target_node, $obs_file);
  }

}


#//--------------------------------------------------------------------------------------------------
#// AddTarget
#// 
#//--------------------------------------------------------------------------------------------------
sub AddTarget($$)
{
  my($doc, $target_name) = @_;

  die "Write me\n";
}

#//--------------------------------------------------------------------------------------------------
#// RemoveTarget
#// 
#//--------------------------------------------------------------------------------------------------
sub RemoveTarget($$)
{
  my($doc, $target_name) = @_;

  die "Write me\n";
}


#//--------------------------------------------------------------------------------------------------
#// GetTargetSetting
#// Get the value for the specified setting in the specified target
#//--------------------------------------------------------------------------------------------------
sub GetTargetSetting($$$)
{
  my($doc, $target_name, $setting_name) = @_;

  my $target_node   = getTargetNode($doc, $target_name);
  return getTargetNodeSetting($target_node, "VALUE");
}


#//--------------------------------------------------------------------------------------------------
#// SetTargetSetting
#// Set the value for the specified setting in the specified target
#//--------------------------------------------------------------------------------------------------
sub SetTargetSetting($$$$)
{
  my($doc, $target_name, $setting_name, $new_value) = @_;

  my $target_node   = getTargetNode($doc, $target_name);
  setTargetNodeSetting($doc, $target_node, "VALUE", $new_value);
}


#//--------------------------------------------------------------------------------------------------
#// GetTargetFilesList
#// Return an array of the files in the target (in filelist order)
#//--------------------------------------------------------------------------------------------------
sub GetTargetFilesList($$)
{
  my($doc, $target_name) = @_;

  my $target_node = getTargetNode($doc, $target_name);

  my @files_list;
    
  my $filelist_node = getFirstChildElement($target_node, "FILELIST");
  unless ($filelist_node) { die "Error: failed to find FILELIST node\n"; }

  my @file_nodes;
  getChildElementsOfType($doc, $filelist_node, "FILE", \@file_nodes);
  
  my $node;
  foreach $node (@file_nodes)
  {
    my $file_name = getChildElementTextContents($node, "PATH");
    push(@files_list, $file_name);
  }
  
  return @files_list;
}


#//--------------------------------------------------------------------------------------------------
#// FileIsInTarget
#// 
#//--------------------------------------------------------------------------------------------------
sub FileIsInTarget($$$)
{
  my($doc, $file_name, $target_name) = @_;

  my $target_node = getTargetNode($doc, $target_name);
  unless ($target_node) { die "Error: no target found called $target_name\n"; }

  my $file_node = GetTargetFileNode($doc, $target_node, $file_name);
  if ($file_node) {
    return 1;
  }
  
  return 0;
}


#//--------------------------------------------------------------------------------------------------
#// GetFileTargetsList
#// Return an array of the targets that a file is in (expensive)
#//--------------------------------------------------------------------------------------------------
sub GetFileTargetsList($$)
{
  my ($doc, $file_name) = @_;

  my @target_list;

  my @targets = GetTargetsList($doc);
  my $target;
  
  foreach $target (@targets)
  {
    if (FileIsInTarget($doc, $file_name, $target))
    {
      push(@target_list, $target);
    }
  }
  
  return @target_list;
}


#//--------------------------------------------------------------------------------------------------
#// GetTargetFileNode
#// 
#//--------------------------------------------------------------------------------------------------
sub GetTargetFileNode($$$)
{
  my($doc, $target_node, $file_name) = @_;

  my $filelist_node = getFirstChildElement($target_node, "FILELIST");
  unless ($filelist_node) { die "Error: failed to find FILELIST node\n"; }
  
  my $file_node = getChildNodeByGrandchildContents($doc, $filelist_node, "FILE", "PATH", $file_name);

  return $file_node;
}


#//--------------------------------------------------------------------------------------------------
#// GetFileInUse
#// Return true if the file is used by any target
#//--------------------------------------------------------------------------------------------------
sub GetFileInUse($$)
{
  my($doc, $file_name) = @_;

  my $targetlist_node = getChildOfDocument($doc, "TARGETLIST");

  my $target_node = $targetlist_node->getFirstChild();
 
  while ($target_node)
  {
    if ($target_node->getNodeTypeName eq "ELEMENT_NODE" &&
        $target_node->getTagName() eq "TARGET")
    {
      # if this is a target node
      my $file_node = GetTargetFileNode($doc, $target_node, $file_name);
      if ($file_node) {
        return 1;   # found it
      }      
    }

    $target_node = $target_node->getNextSibling();
  }
  
  # not found
  return 0;
}

#//--------------------------------------------------------------------------------------------------
#// getChildOfDocument
#//--------------------------------------------------------------------------------------------------
sub getChildOfDocument($$)
{
  my($doc, $child_type) = @_;

  return getFirstChildElement($doc->getDocumentElement(), $child_type);
}


#//--------------------------------------------------------------------------------------------------
#// getFirstChildElement
#//--------------------------------------------------------------------------------------------------
sub getFirstChildElement($$)
{
  my($node, $element_name) = @_;

  my $found_node;
  
  unless ($node) { die "getFirstChildElement called with empty node\n"; }
  
  #look for the first "element_name" child

  my $child_node = $node->getFirstChild();
 
  while ($child_node)
  {
    if ($child_node->getNodeTypeName eq "ELEMENT_NODE" &&
        $child_node->getTagName() eq $element_name)
    {
      $found_node = $child_node;
      last;
    }

    $child_node = $child_node->getNextSibling();
  }
   
  return $found_node;
}


#//--------------------------------------------------------------------------------------------------
#// getChildElementsOfType
#// 
#// Return an array of refs to child nodes of the given type
#//--------------------------------------------------------------------------------------------------
sub getChildElementsOfType($$$$)
{
  my($doc, $node, $child_type, $array_ref) = @_;

  my $child_node = $node->getFirstChild();
 
  while ($child_node)
  {
    if ($child_node->getNodeTypeName eq "ELEMENT_NODE" &&
        $child_node->getTagName() eq $child_type)
    {
      push(@$array_ref, $child_node);
    }

    $child_node = $child_node->getNextSibling();
  }

}

#//--------------------------------------------------------------------------------------------------
#// getChildElementTextContents
#//--------------------------------------------------------------------------------------------------
#
# Given <FOOPY><NERD>Hi!</NERD></FOOPY>, where $node is <FOOPY>,
# returns "Hi!". If > 1 <NERD> node, returns the contents of the first.
#
sub getChildElementTextContents($$)
{
  my($node, $tag_name) = @_;

  my $first_element = getFirstChildElement($node, $tag_name);  
  my $text_node     = $first_element->getFirstChild();
  
  my $text_contents = "";
  
  # concat adjacent text nodes
  while ($text_node)
  {
    if ($text_node->getNodeTypeName() ne "TEXT_NODE")
    {
      last;
    }

    $text_contents = $text_contents.$text_node->getData();
    $text_node = $text_node->getNextSibling();
  }

  return $text_contents;
}

#//--------------------------------------------------------------------------------------------------
#// setChildElementTextContents
#//--------------------------------------------------------------------------------------------------
sub setChildElementTextContents($$$$)
{
  my($doc, $node, $tag_name, $contents_text) = @_;
  
  my $first_element = getFirstChildElement($node, $tag_name);  
  my $new_text_node = $doc->createTextNode($contents_text);
  
  # replace all child elements with a text element
  removeAllChildren($first_element);
  
  $first_element->appendChild($new_text_node);
}


#//--------------------------------------------------------------------------------------------------
#// getChildNodeByContents
#// 
#// Consider <foo><bar><baz>Foopy</baz></bar><bar><baz>Loopy</baz></bar></foo>
#// This function, when called with getChildNodeByContents($foonode, "bar", "baz", "Loopy")
#// returns the second <bar> node.
#//--------------------------------------------------------------------------------------------------
sub getChildNodeByGrandchildContents($$$$$)
{
  my($doc, $node, $child_type, $gc_type, $gc_contents) = @_;    # gc = grandchild

  my $found_node;
  my $child_node = $node->getFirstChild();
  while ($child_node)
  {
    if ($child_node->getNodeTypeName eq "ELEMENT_NODE" &&
        $child_node->getTagName() eq $child_type)
    {
      # check for a child of this node of type 
      my $child_contents = getChildElementTextContents($child_node, $gc_type);

      if ($child_contents eq $gc_contents)
      {
        $found_node = $child_node;
        last;
      }
    }

    $child_node = $child_node->getNextSibling();
  }

  return $found_node;
}


#//--------------------------------------------------------------------------------------------------
#// getTargetNode
#//--------------------------------------------------------------------------------------------------
sub getTargetNode($$)
{
  my($doc, $target_name) = @_;

  my $targetlist_node = getChildOfDocument($doc, "TARGETLIST");
  return getChildNodeByGrandchildContents($doc, $targetlist_node, "TARGET", "NAME", $target_name);
}


#//--------------------------------------------------------------------------------------------------
#// getTargetNamedSettingNode
#//--------------------------------------------------------------------------------------------------
sub getTargetNamedSettingNode($$)
{
  my($target_node, $setting_name) = @_;

  my $setting_node;

  my $settinglist_node = getFirstChildElement($target_node, "SETTINGLIST");
  my $child_node = $settinglist_node->getFirstChild();
 
  while ($child_node)
  {
    if ($child_node->getNodeTypeName ne "ELEMENT_NODE")
    {
      $child_node = $child_node->getNextSibling();
      next;
    }
    
    if ($child_node->getTagName() eq "SETTING")
    {
      my $set_name = getChildElementTextContents($child_node, "NAME");

      if ($set_name eq $setting_name)
      {
        $setting_node = $child_node;
        last;
      }
    }

    $child_node = $child_node->getNextSibling();
  }

  return $setting_node;
}


#//--------------------------------------------------------------------------------------------------
#// getTargetNodeSetting
#//--------------------------------------------------------------------------------------------------
sub getTargetNodeSetting($$)
{
  my($target_node, $setting_name) = @_;
  
  my $setting_node  = getTargetNamedSettingNode($target_node, $setting_name);
  return getChildElementTextContents($setting_node, "VALUE");
}


#//--------------------------------------------------------------------------------------------------
#// setTargetNodeSetting
#//--------------------------------------------------------------------------------------------------
sub setTargetNodeSetting($$$$)
{
  my($doc, $target_node, $setting_name, $new_value) = @_;

  my $setting_node  = getTargetNamedSettingNode($target_node, $setting_name);

  setChildElementTextContents($doc, $setting_node, "VALUE", $new_value);
}


#//--------------------------------------------------------------------------------------------------
#// elementInArray
#//--------------------------------------------------------------------------------------------------
sub elementInArray($$)
{
  my($element, $array) = @_;
  my $test;
  foreach $test (@$array)
  {
    if ($test eq $element) {
      return 1;
    }
  }
  return 0;
}


#//--------------------------------------------------------------------------------------------------
#// removeAllChildren
#//--------------------------------------------------------------------------------------------------
sub removeAllChildren($)
{
  my($node) = @_;
  
  my $child_node = $node->getFirstChild();
  
  while ($child_node)
  {
    $node->removeChild($child_node);
    $child_node = $node->getFirstChild();
  }
}


#//--------------------------------------------------------------------------------------------------
#// dumpNodeData
#//--------------------------------------------------------------------------------------------------
sub dumpNodeData($)
{
  my($node) = @_;
  
  unless ($node) { die "Null node passed to dumpNodeData\n"; }
  
  print "Dumping node $node\n";
  
  my($node_type) = $node->getNodeTypeName();
  
  if ($node_type eq "ELEMENT_NODE")
  {
    my($node_name) = $node->getTagName();
    print "Element $node_name\n";
  }
  elsif ($node_type eq "TEXT_NODE")
  {
    my($node_data) = $node->getData;
    # my(@node_vals) = unpack("C*", $node_data);
    print "Text '$node_data'\n";    # may contain LF chars
  }
  else
  {
    print "Node $node_type\n";
  }

}

#//--------------------------------------------------------------------------------------------------
#// dumpNodeTree
#//--------------------------------------------------------------------------------------------------
sub dumpNodeTree($)
{
  my($node) = @_;
  
  my($child_node) = $node->getFirstChild();
  
  unless ($child_node) { return; }

  # recurse
  dumpNodeData($child_node);
  
  # then go through child nodes
  while ($child_node)
  {
    dumpNodeTree($child_node);
  
    $child_node = $child_node->getNextSibling();
  }
}




1;

