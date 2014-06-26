#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
    "/c", "ATLVersionCheck", "/c", "ATLVulnCheck", "/c", "SharedSectionCheck", "/c", "APTCACheck", "/c", "NXCheck",
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
  print "Error count: %d" % proc.returncode
  print "TEST-UNEXPECTED-FAIL | autobinscope.py | %s is missing a needed Windows protection, such as /GS or ASLR" % binary_path
  logfile = open(log_file_path, "r")
  for line in logfile:
    print(line),
else:
  print "TEST-PASS | autobinscope.py | %s succeeded" % binary_path
