#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script exists to generate the Certificate Authority and server
# certificates used for SSL testing in Mochitest. The already generated
# certs are located at $topsrcdir/build/pgo/certs/ .

import mozinfo
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile

from mozbuild.base import MozbuildObject
from mozfile import NamedTemporaryFile
from mozprofile.permissions import ServerLocations

dbFiles = [
  re.compile("^cert[0-9]+\.db$"),
  re.compile("^key[0-9]+\.db$"),
  re.compile("^secmod\.db$")
]

def unlinkDbFiles(path):
  for root, dirs, files in os.walk(path):
    for name in files:
      for dbFile in dbFiles:
        if dbFile.match(name) and os.path.exists(os.path.join(root, name)):
          os.unlink(os.path.join(root, name))

def dbFilesExist(path):
  for root, dirs, files in os.walk(path):
    for name in files:
      for dbFile in dbFiles:
        if dbFile.match(name) and os.path.exists(os.path.join(root, name)):
          return True
  return False


def runUtil(util, args, inputdata = None):
  env = os.environ.copy()
  if mozinfo.os == "linux":
    pathvar = "LD_LIBRARY_PATH"
    app_path = os.path.dirname(util)
    if pathvar in env:
      env[pathvar] = "%s%s%s" % (app_path, os.pathsep, env[pathvar])
    else:
      env[pathvar] = app_path
  proc = subprocess.Popen([util] + args, env=env,
                          stdin=subprocess.PIPE if inputdata else None)
  proc.communicate(inputdata)
  return proc.returncode


def createRandomFile(randomFile):
  for count in xrange(0, 2048):
    randomFile.write(chr(random.randint(0, 255)))


def createCertificateAuthority(build, srcDir):
  certutil = build.get_binary_path(what="certutil")
  pk12util = build.get_binary_path(what="pk12util")

  #TODO: mozfile.TemporaryDirectory
  tempDbDir = tempfile.mkdtemp()
  with NamedTemporaryFile() as pwfile, NamedTemporaryFile() as rndfile:
    pgoCAModulePathSrc = os.path.join(srcDir, "pgoca.p12")
    pgoCAPathSrc = os.path.join(srcDir, "pgoca.ca")

    pwfile.write("\n")

    # Create temporary certification database for CA generation
    status = runUtil(certutil, ["-N", "-d", tempDbDir, "-f", pwfile.name])
    if status:
      return status

    createRandomFile(rndfile)
    status = runUtil(certutil, ["-S", "-d", tempDbDir, "-s", "CN=Temporary Certificate Authority, O=Mozilla Testing, OU=Profile Guided Optimization", "-t", "C,,", "-x", "-m", "1", "-v", "120", "-n", "pgo temporary ca", "-2", "-f", pwfile.name, "-z", rndfile.name], "Y\n0\nN\n")
    if status:
      return status

    status = runUtil(certutil, ["-L", "-d", tempDbDir, "-n", "pgo temporary ca", "-a", "-o", pgoCAPathSrc, "-f", pwfile.name])
    if status:
      return status

    status = runUtil(pk12util, ["-o", pgoCAModulePathSrc, "-n", "pgo temporary ca", "-d", tempDbDir, "-w", pwfile.name, "-k", pwfile.name])
    if status:
      return status

  shutil.rmtree(tempDbDir)
  return 0


def createSSLServerCertificate(build, srcDir):
  certutil = build.get_binary_path(what="certutil")
  pk12util = build.get_binary_path(what="pk12util")

  with NamedTemporaryFile() as pwfile, NamedTemporaryFile() as rndfile:
    pgoCAPath = os.path.join(srcDir, "pgoca.p12")

    pwfile.write("\n")

    if not dbFilesExist(srcDir):
      # Make sure all DB files from src are really deleted
      unlinkDbFiles(srcDir)

      # Create certification database for ssltunnel
      status = runUtil(certutil, ["-N", "-d", srcDir, "-f", pwfile.name])
      if status:
        return status

      status = runUtil(pk12util, ["-i", pgoCAPath, "-w", pwfile.name, "-d", srcDir, "-k", pwfile.name])
      if status:
        return status

    # Generate automatic certificate
    locations = ServerLocations(os.path.join(build.topsrcdir,
                                             "build", "pgo",
                                             "server-locations.txt"))
    iterator = iter(locations)

    # Skips the first entry, I don't know why: bug 879740
    iterator.next()

    locationsParam = ""
    firstLocation = ""
    for loc in iterator:
      if loc.scheme == "https" and "nocert" not in loc.options:
        customCertOption = False
        customCertRE = re.compile("^cert=(?:\w+)")
        for option in loc.options:
          match = customCertRE.match(option)
          if match:
            customCertOption = True
            break

        if not customCertOption:
          if len(locationsParam) > 0:
            locationsParam += ","
          locationsParam += loc.host

          if firstLocation == "":
            firstLocation = loc.host

    if not firstLocation:
      print "Nothing to generate, no automatic secure hosts specified"
    else:
      createRandomFile(rndfile)

      runUtil(certutil, ["-D", "-n", "pgo server certificate", "-d", srcDir, "-z", rndfile.name, "-f", pwfile.name])
      # Ignore the result, the certificate may not be present when new database is being built

      status = runUtil(certutil, ["-S", "-s", "CN=%s" % firstLocation, "-t", "Pu,,", "-c", "pgo temporary ca", "-m", "2", "-8", locationsParam, "-v", "120", "-n", "pgo server certificate", "-d", srcDir, "-z", rndfile.name, "-f", pwfile.name])
      if status:
        return status

  return 0

if len(sys.argv) == 1:
  print "Specify --gen-server or --gen-ca"
  sys.exit(1)

build = MozbuildObject.from_environment()
certdir = os.path.join(build.topsrcdir, "build", "pgo", "certs")
if sys.argv[1] == "--gen-server":
  certificateStatus = createSSLServerCertificate(build, certdir)
  if certificateStatus:
    print "TEST-UNEXPECTED-FAIL | SSL Server Certificate generation"

  sys.exit(certificateStatus)

if sys.argv[1] == "--gen-ca":
  certificateStatus = createCertificateAuthority(build, certdir)
  if certificateStatus:
    print "TEST-UNEXPECTED-FAIL | Certificate Authority generation"
  else:
    print "\n\n"
    print "==================================================="
    print " IMPORTANT:"
    print " To use this new certificate authority in tests"
    print " run 'make' at testing/mochitest"
    print "==================================================="

  sys.exit(certificateStatus)

print "Invalid option specified"
sys.exit(1)
