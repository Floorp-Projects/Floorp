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
# The Original Code is Test Automation Framework.
#
# The Initial Developer of the Original Code is Joel Maher.
#
# Portions created by the Initial Developer are Copyright (C) 2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Joel Maher <joel.maher@gmail.com> (Original Developer)
#   Clint Talbert <cmtalbert@gmail.com>
#   Mark Cote <mcote@mozilla.com>
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

import time
import hashlib
import socket
import os
import re

class FileError(Exception):
  " Signifies an error which occurs while doing a file operation."

  def __init__(self, msg = ''):
    self.msg = msg

  def __str__(self):
    return self.msg

class DMError(Exception):
  "generic devicemanager exception."

  def __init__(self, msg= ''):
    self.msg = msg

  def __str__(self):
    return self.msg


class DeviceManager:
  # external function
  # returns:
  #  success: True
  #  failure: False
  def pushFile(self, localname, destname):
    assert 0 == 1
    return False

  # external function
  # returns:
  #  success: directory name
  #  failure: None
  def mkDir(self, name):
      assert 0 == 1
      return None

  # make directory structure on the device
  # external function
  # returns:
  #  success: directory structure that we created
  #  failure: None
  def mkDirs(self, filename):
      assert 0 == 1
      return None

  # push localDir from host to remoteDir on the device
  # external function
  # returns:
  #  success: remoteDir
  #  failure: None
  def pushDir(self, localDir, remoteDir):
    assert 0 == 1
    return None

  # external function
  # returns:
  #  success: True
  #  failure: False
  def dirExists(self, dirname):
    assert 0 == 1
    return False

  # Because we always have / style paths we make this a lot easier with some
  # assumptions
  # external function
  # returns:
  #  success: True
  #  failure: False
  def fileExists(self, filepath):
    assert 0 == 1
    return False

  # list files on the device, requires cd to directory first
  # external function
  # returns:
  #  success: array of filenames, ['file1', 'file2', ...]
  #  failure: []
  def listFiles(self, rootdir):
    assert 0 == 1
    return []

  # external function
  # returns:
  #  success: output of telnet, i.e. "removing file: /mnt/sdcard/tests/test.txt"
  #  failure: None
  def removeFile(self, filename):
    assert 0 == 1
    return False

  # does a recursive delete of directory on the device: rm -Rf remoteDir
  # external function
  # returns:
  #  success: output of telnet, i.e. "removing file: /mnt/sdcard/tests/test.txt"
  #  failure: None
  def removeDir(self, remoteDir):
    assert 0 == 1
    return None

  # external function
  # returns:
  #  success: array of process tuples
  #  failure: []
  def getProcessList(self):
    assert 0 == 1
    return []

  # external function
  # returns:
  #  success: pid
  #  failure: None
  def fireProcess(self, appname, failIfRunning=False):
    assert 0 == 1
    return None

  # external function
  # returns:
  #  success: output filename
  #  failure: None
  def launchProcess(self, cmd, outputFile = "process.txt", cwd = '', env = '', failIfRunning=False):
    assert 0 == 1
    return None

  # loops until 'process' has exited or 'timeout' seconds is reached
  # loop sleeps for 'interval' seconds between iterations
  # external function
  # returns:
  #  success: [file contents, None]
  #  failure: [None, None]
  def communicate(self, process, timeout = 600, interval = 5):
    timed_out = True
    if (timeout > 0):
      total_time = 0
      while total_time < timeout:
        time.sleep(interval)
        if self.processExist(process) == None:
          timed_out = False
          break
        total_time += interval

    if (timed_out == True):
      return [None, None]

    return [self.getFile(process, "temp.txt"), None]

  # iterates process list and returns pid if exists, otherwise None
  # external function
  # returns:
  #  success: pid
  #  failure: None
  def processExist(self, appname):
    pid = None

    #filter out extra spaces
    parts = filter(lambda x: x != '', appname.split(' '))
    appname = ' '.join(parts)

    #filter out the quoted env string if it exists
    #ex: '"name=value;name2=value2;etc=..." process args' -> 'process args'
    parts = appname.split('"')
    if (len(parts) > 2):
      appname = ' '.join(parts[2:]).strip()
  
    pieces = appname.split(' ')
    parts = pieces[0].split('/')
    app = parts[-1]
    procre = re.compile('.*' + app + '.*')

    procList = self.getProcessList()
    if (procList == []):
      return None
      
    for proc in procList:
      if (procre.match(proc[1])):
        pid = proc[0]
        break
    return pid

  # external function
  # returns:
  #  success: output from testagent
  #  failure: None
  def killProcess(self, appname):
    assert 0 == 1
    return None

  # external function
  # returns:
  #  success: filecontents
  #  failure: None
  def catFile(self, remoteFile):
    assert 0 == 1
    return None

  # external function
  # returns:
  #  success: output of pullfile, string
  #  failure: None
  def pullFile(self, remoteFile):
    assert 0 == 1
    return None

  # copy file from device (remoteFile) to host (localFile)
  # external function
  # returns:
  #  success: output of pullfile, string
  #  failure: None
  def getFile(self, remoteFile, localFile = ''):
    assert 0 == 1
    return None

  # copy directory structure from device (remoteDir) to host (localDir)
  # external function
  # checkDir exists so that we don't create local directories if the
  # remote directory doesn't exist but also so that we don't call isDir
  # twice when recursing.
  # returns:
  #  success: list of files, string
  #  failure: None
  def getDirectory(self, remoteDir, localDir, checkDir=True):
    assert 0 == 1
    return None

  # external function
  # returns:
  #  success: True
  #  failure: False
  #  Throws a FileError exception when null (invalid dir/filename)
  def isDir(self, remotePath):
    assert 0 == 1
    return False

  # true/false check if the two files have the same md5 sum
  # external function
  # returns:
  #  success: True
  #  failure: False
  def validateFile(self, remoteFile, localFile):
    assert 0 == 1
    return False

  # return the md5 sum of a remote file
  # internal function
  # returns:
  #  success: MD5 hash for given filename
  #  failure: None
  def getRemoteHash(self, filename):
    assert 0 == 1
    return None

  # return the md5 sum of a file on the host
  # internal function
  # returns:
  #  success: MD5 hash for given filename
  #  failure: None
  def getLocalHash(self, filename):
    file = open(filename, 'rb')
    if (file == None):
      return None

    try:
      mdsum = hashlib.md5()
    except:
      return None

    while 1:
      data = file.read(1024)
      if not data:
        break
      mdsum.update(data)

    file.close()
    hexval = mdsum.hexdigest()
    if (self.debug >= 3): print "local hash returned: '" + hexval + "'"
    return hexval
  # Gets the device root for the testing area on the device
  # For all devices we will use / type slashes and depend on the device-agent
  # to sort those out.  The agent will return us the device location where we
  # should store things, we will then create our /tests structure relative to
  # that returned path.
  # Structure on the device is as follows:
  # /tests
  #       /<fennec>|<firefox>  --> approot
  #       /profile
  #       /xpcshell
  #       /reftest
  #       /mochitest
  #
  # external function
  # returns:
  #  success: path for device root
  #  failure: None
  def getDeviceRoot(self):
    assert 0 == 1
    return None

  # Either we will have /tests/fennec or /tests/firefox but we will never have
  # both.  Return the one that exists
  # TODO: ensure we can support org.mozilla.firefox
  # external function
  # returns:
  #  success: path for app root
  #  failure: None
  def getAppRoot(self):
    devroot = self.getDeviceRoot()
    if (devroot == None):
      return None

    if (self.dirExists(devroot + '/fennec')):
      return devroot + '/fennec'
    elif (self.dirExists(devroot + '/firefox')):
      return devroot + '/firefox'
    elif (self.dirExsts('/data/data/org.mozilla.fennec')):
      return 'org.mozilla.fennec'
    elif (self.dirExists('/data/data/org.mozilla.firefox')):
      return 'org.mozilla.firefox'
    elif (self.dirExists('/data/data/org.mozilla.fennec_aurora')):
      return 'org.mozilla.fennec_aurora'
    elif (self.dirExists('/data/data/org.mozilla.firefox_beta')):
      return 'org.mozilla.firefox_beta'

    # Failure (either not installed or not a recognized platform)
    return None

  # Gets the directory location on the device for a specific test type
  # Type is one of: xpcshell|reftest|mochitest
  # external function
  # returns:
  #  success: path for test root
  #  failure: None
  def getTestRoot(self, type):
    devroot = self.getDeviceRoot()
    if (devroot == None):
      return None

    if (re.search('xpcshell', type, re.I)):
      self.testRoot = devroot + '/xpcshell'
    elif (re.search('?(i)reftest', type)):
      self.testRoot = devroot + '/reftest'
    elif (re.search('?(i)mochitest', type)):
      self.testRoot = devroot + '/mochitest'
    return self.testRoot

  # Sends a specific process ID a signal code and action.
  # For Example: SIGINT and SIGDFL to process x
  def signal(self, processID, signalType, signalAction):
    # currently not implemented in device agent - todo
    pass

  # Get a return code from process ending -- needs support on device-agent
  def getReturnCode(self, processID):
    # TODO: make this real
    return 0

  # external function
  # returns:
  #  success: output of unzip command
  #  failure: None
  def unpackFile(self, filename):
    return None

  # external function
  # returns:
  #  success: status from test agent
  #  failure: None
  def reboot(self, ipAddr=None, port=30000):
    assert 0 == 1
    return None

  # validate localDir from host to remoteDir on the device
  # external function
  # returns:
  #  success: True
  #  failure: False
  def validateDir(self, localDir, remoteDir):
    if (self.debug >= 2): print "validating directory: " + localDir + " to " + remoteDir
    for root, dirs, files in os.walk(localDir):
      parts = root.split(localDir)
      for file in files:
        remoteRoot = remoteDir + '/' + parts[1]
        remoteRoot = remoteRoot.replace('/', '/')
        if (parts[1] == ""): remoteRoot = remoteDir
        remoteName = remoteRoot + '/' + file
        if (self.validateFile(remoteName, os.path.join(root, file)) <> True):
            return False
    return True

  # Returns information about the device:
  # Directive indicates the information you want to get, your choices are:
  # os - name of the os
  # id - unique id of the device
  # uptime - uptime of the device
  # systime - system time of the device
  # screen - screen resolution
  # memory - memory stats
  # process - list of running processes (same as ps)
  # disk - total, free, available bytes on disk
  # power - power status (charge, battery temp)
  # all - all of them - or call it with no parameters to get all the information
  # returns:
  #   success: dict of info strings by directive name
  #   failure: {}
  def getInfo(self, directive=None):
    assert 0 == 1
    return {}

  # external function
  # returns:
  #  success: output from agent for inst command
  #  failure: None
  def installApp(self, appBundlePath, destPath=None):
    assert 0 == 1
    return None

  # external function
  # returns:
  #  success: True
  #  failure: None
  def uninstallAppAndReboot(self, appName, installPath=None):
    assert 0 == 1
    return None

  # external function
  # returns:
  #  success: text status from command or callback server
  #  failure: None
  def updateApp(self, appBundlePath, processName=None, destPath=None, ipAddr=None, port=30000):
    assert 0 == 1
    return None

  # external function
  # returns:
  #  success: time in ms
  #  failure: None
  def getCurrentTime(self):
    assert 0 == 1
    return None


class NetworkTools:
  def __init__(self):
    pass

  # Utilities to get the local ip address
  def getInterfaceIp(self, ifname):
    if os.name != "nt":
      import fcntl
      import struct
      s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
      return socket.inet_ntoa(fcntl.ioctl(
                              s.fileno(),
                              0x8915,  # SIOCGIFADDR
                              struct.pack('256s', ifname[:15])
                              )[20:24])
    else:
      return None

  def getLanIp(self):
    try:
      ip = socket.gethostbyname(socket.gethostname())
    except socket.gaierror:
      ip = socket.gethostbyname(socket.gethostname() + ".local") # for Mac OS X
    if ip.startswith("127.") and os.name != "nt":
      interfaces = ["eth0","eth1","eth2","wlan0","wlan1","wifi0","ath0","ath1","ppp0"]
      for ifname in interfaces:
        try:
          ip = self.getInterfaceIp(ifname)
          break;
        except IOError:
          pass
    return ip

  # Gets an open port starting with the seed by incrementing by 1 each time
  def findOpenPort(self, ip, seed):
    try:
      s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
      s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
      connected = False
      if isinstance(seed, basestring):
        seed = int(seed)
      maxportnum = seed + 5000 # We will try at most 5000 ports to find an open one
      while not connected:
        try:
          s.bind((ip, seed))
          connected = True
          s.close()
          break
        except:          
          if seed > maxportnum:
            print "Could not find open port after checking 5000 ports"
          raise
        seed += 1
    except:
      print "Socket error trying to find open port"
        
    return seed

