#!/usr/bin/env python

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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   imelven@mozilla.com
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

# run Microsoft's Binscope tool (http://www.microsoft.com/download/en/details.aspx?id=11910)
# against a fresh Windows build. output a 'binscope.log' file with full details
# of the run and appropriate strings to integrate with the buildbots

# from the docs : "The error code returned when running under the command line is equal 
# to the number of failures the tool reported plus the number of errors. BinScope will return 
# 0 only if there are no errors or failures."

# the symbol dir should point to the symbol dir hierarchy created
# via running make buildsymbols in a windows build's objdir

import sys
import subprocess
import os

BINSCOPE_OUTPUT_LOGFILE = r".\binscope_xml_output.log"

# usage
if len(sys.argv) < 3:
  print """usage : autobinscope.by path_to_binary path_to_symbols [log_file_path]"
		log_file_path is optional, log will be written to .\binscope_xml_output.log by default"""
  sys.exit(0)

binary_path = sys.argv[1]
symbol_path = sys.argv[2]

if len(sys.argv) == 4:
  log_file_path = sys.argv[3]
else:
  log_file_path = BINSCOPE_OUTPUT_LOGFILE
  
# execute binscope against the binary, using the BINSCOPE environment
# variable as the path to binscope.exe
try:
  binscope_path = os.environ['BINSCOPE']
except KeyError:
  print "BINSCOPE environment variable is not set, can't check DEP/ASLR etc. status."
  sys.exit(0)
  
try:    
  proc = subprocess.Popen([binscope_path, "/target", binary_path,
    "/output", log_file_path, "/sympath", symbol_path,
    "/c", "ATLVersionCheck", "/c", "ATLVulnCheck", "/c", "FunctionPointersCheck",
    "/c", "SharedSectionCheck", "/c", "APTCACheck", "/c", "NXCheck",
    "/c", "GSCheck", "/c", "GSFunctionSafeBuffersCheck", "/c", "GSFriendlyInitCheck",
    "/c", "CompilerVersionCheck", "/c", "SafeSEHCheck", "/c", "SNCheck",
    "/c", "DBCheck"], stdout=subprocess.PIPE)

except WindowsError, (errno, strerror): 
  if errno != 2 and errno != 3:
    print "Unexpected error ! \nError " + str(errno) + " : " + strerror + "\nExiting !\n"
    sys.exit(0)
  else:
    print "Could not locate binscope at location : %s\n" % binscope_path
    print "Binscope wasn't installed or the BINSCOPE env variable wasn't set correctly, skipping this check and exiting..."
    sys.exit(0)

proc.wait()

output = proc.communicate()[0]

# is this a PASS or a FAIL ? 
if proc.returncode != 0:
  print "TEST-UNEXPECTED-FAIL | autobinscope.py | %s is missing a needed Windows protection, such as /GS or ASLR" % binary_path
else:
  print "TEST-PASS | autobinscope.py | %s succeeded" % binary_path



