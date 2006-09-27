#! /usr/bin/env perl
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
# The Original Code is Java XPCOM Bindings.
#
# The Initial Developer of the Original Code is
# Michal Ceresna.
# Portions created by the Initial Developer are Copyright (C) 2005
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Michal Ceresna (ceresna@dbai.tuwien.ac.at)
#   Javier Pedemonte (jhpedemonte@gmail.com)
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

# Generates XPCOMError.java from xpcom/base/nsError.h
#
# usage: perl gen-nsErrors.pl < <topsrcdir>/xpcom/base/nsError.h > XPCOMError.java


print "/* ***** BEGIN LICENSE BLOCK *****\n";
print " * Version: MPL 1.1/GPL 2.0/LGPL 2.1\n";
print " *\n";
print " * The contents of this file are subject to the Mozilla Public License Version\n";
print " * 1.1 (the \"License\"); you may not use this file except in compliance with\n";
print " * the License. You may obtain a copy of the License at\n";
print " * http://www.mozilla.org/MPL/\n";
print " *\n";
print " * Software distributed under the License is distributed on an \"AS IS\" basis,\n";
print " * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License\n";
print " * for the specific language governing rights and limitations under the\n";
print " * License.\n";
print " *\n";
print " * The Original Code is mozilla.org code.\n";
print " *\n";
print " * The Initial Developer of the Original Code is\n";
print " * Netscape Communications Corporation.\n";
print " * Portions created by the Initial Developer are Copyright (C) 1998\n";
print " * the Initial Developer. All Rights Reserved.\n";
print " *\n";
print " * Contributor(s):\n";
print " *\n";
print " * Alternatively, the contents of this file may be used under the terms of\n";
print " * either of the GNU General Public License Version 2 or later (the \"GPL\"),\n";
print " * or the GNU Lesser General Public License Version 2.1 or later (the \"LGPL\"),\n";
print " * in which case the provisions of the GPL or the LGPL are applicable instead\n";
print " * of those above. If you wish to allow use of your version of this file only\n";
print " * under the terms of either the GPL or the LGPL, and not to allow others to\n";
print " * use your version of this file under the terms of the MPL, indicate your\n";
print " * decision by deleting the provisions above and replace them with the notice\n";
print " * and other provisions required by the GPL or the LGPL. If you do not delete\n";
print " * the provisions above, a recipient may use your version of this file under\n";
print " * the terms of any one of the MPL, the GPL or the LGPL.\n";
print " *\n";
print " * ***** END LICENSE BLOCK ***** */\n";

print "\n";
print "package org.mozilla.xpcom;\n";
print "\n\n";

print "/**\n";
print " * Mozilla error codes.\n";
print " *\n";
print " * THIS FILE GENERATED FROM mozilla/xpcom/base/nsError.h.\n";
print " * PLEASE SEE THAT FILE FOR FULL DOCUMENTATION.\n";
print " */\n";
print "public interface XPCOMError {\n";

while (<STDIN>) {
  $line = $_;

  if ($prevline) {
    $_ = $prevline.$_;
  }
  if (/(.*)\\$/) {
    #splitted line
    $prevline = $1;
    next;
  }
  $prevline = "";

  if (/^\s*#define\s+(NS_[A-Z_]+)\s+(0x)?([0-9a-fA-F]+)\s*$/) {
    #define NS_ERROR_MODULE_XPCOM      1
    #define NS_ERROR_MODULE_BASE_OFFSET 0x45
    print "  public static final long $1 = $2$3L;\n";
  }
  elsif (/^\s*#define\s+(NS_[A-Z_]+)\s+\((NS_[A-Z_]+)\s+\+\s+(0x)?([0-9a-fA-F]+)\s*\)\s*/) {
    #define NS_ERROR_NOT_INITIALIZED           (NS_ERROR_BASE + 1)
    #define NS_ERROR_FACTORY_EXISTS            (NS_ERROR_BASE + 0x100)
    print "  public static final long $1 = $2 + $3$4L;\n";
  }
  elsif (/^\s*#define\s+(NS_[A-Z_]+)\s+(NS_[A-Z_]+)\s\s*/) {
    #define NS_ERROR_NO_INTERFACE              NS_NOINTERFACE
    print "  public static final long $1 = $2;\n";
  }
  elsif (/^\s*#define\s+(NS_[A-Z_]+)\s+\(\(nsresult\)\s*(0x)?([0-9a-fA-F]+)L?\)\s*$/) { 
    #define NS_ERROR_BASE                      ((nsresult) 0xC1F30000)
    #define NS_ERROR_ABORT                     ((nsresult) 0x80004004L)
    print "  public static final long $1 = $2$3L;\n";
  }
  elsif (/^\s*#define\s+(NS_[A-Z_]+)\s+NS_ERROR_GENERATE_FAILURE\s*\(\s*(NS_[A-Z_]+)\s*,\s*([0-9]+)\s*\)\s*$/) { 
    #define NS_BASE_STREAM_CLOSED         NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_BASE, 2)
    $module = $2;
    $code = $3;
    print "  public static final long $1 = ((NS_ERROR_SEVERITY_ERROR<<31) | (($module+NS_ERROR_MODULE_BASE_OFFSET)<<16) | $code);\n";
  }
  elsif (/^\s*#define\s+(NS_[A-Z_]+)\s+NS_ERROR_GENERATE_SUCCESS\s*\(\s*(NS_[A-Z_]+)\s*,\s*([0-9]+)\s*\)\s*$/) { 
    #define NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA   NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_XPCOM,  1)
    $module = $2;
    $code = $3;
    print "  public static final long $1 = ((NS_ERROR_SEVERITY_SUCCESS<<31) | (($module+NS_ERROR_MODULE_BASE_OFFSET)<<16) | $code);\n";
  }
  elsif (/^\s*\/\*(.*)\*\/\s*$/ && !/^\s*\/\*@[\{\}]\*\/\s*$/ &&
         !/^\s*\/\*\ -\*- Mode:/) {
    #single line comment, but not 
    #/*@{*/, /*@}*/, 
    #/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
    print "  /*$1*/\n";
  }
  elsif (/^\s*$/) {
    #empty line, but write only the first
    #line from a sequence of empty lines
    if (!$wasEmpty) {
      print "\n";
    }
    $wasEmpty = 1;
    next;
  }
  else {
    next;
  }

  $wasEmpty = 0;
}

print "}\n";

