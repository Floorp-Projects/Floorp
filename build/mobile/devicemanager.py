# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import time
import hashlib
import socket
import os
import re
import StringIO

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

def abstractmethod(method):
  line = method.func_code.co_firstlineno
  filename = method.func_code.co_filename
  def not_implemented(*args, **kwargs):
    raise NotImplementedError('Abstract method %s at File "%s", line %s '
                              'should be implemented by a concrete class' %
                              (repr(method), filename,line))
  return not_implemented

class DeviceManager:

  @abstractmethod
  def shell(self, cmd, outputfile, env=None, cwd=None):
    """
    executes shell command on device
    returns:
    success: Return code from command
    failure: None
    """

  @abstractmethod
  def pushFile(self, localname, destname):
    """
    external function
    returns:
    success: True
    failure: False
    """
    
  @abstractmethod
  def mkDir(self, name):
    """
    external function
    returns:
    success: directory name
    failure: None
    """
    
  @abstractmethod
  def mkDirs(self, filename):
    """
    make directory structure on the device
    external function
    returns:
    success: directory structure that we created
    failure: None
    """
    
  @abstractmethod
  def pushDir(self, localDir, remoteDir):
    """
    push localDir from host to remoteDir on the device
    external function
    returns:
    success: remoteDir
    failure: None
    """

  @abstractmethod
  def dirExists(self, dirname):
    """
    external function
    returns:
    success: True
    failure: False
    """
    
  @abstractmethod
  def fileExists(self, filepath):
    """
    Because we always have / style paths we make this a lot easier with some
    assumptions
    external function
    returns:
    success: True
    failure: False
    """
    
  @abstractmethod
  def listFiles(self, rootdir):
    """
    list files on the device, requires cd to directory first
    external function
    returns:
    success: array of filenames, ['file1', 'file2', ...]
    failure: None
    """
  
  @abstractmethod
  def removeFile(self, filename):
    """
    external function
    returns:
    success: output of telnet, i.e. "removing file: /mnt/sdcard/tests/test.txt"
    failure: None
    """
    
  @abstractmethod
  def removeDir(self, remoteDir):
    """
    does a recursive delete of directory on the device: rm -Rf remoteDir
    external function
    returns:
    success: output of telnet, i.e. "removing file: /mnt/sdcard/tests/test.txt"
    failure: None
    """
    
  @abstractmethod
  def getProcessList(self):
    """
    external function
    returns:
    success: array of process tuples
    failure: None
    """

  @abstractmethod
  def fireProcess(self, appname, failIfRunning=False):
    """
    external function
    DEPRECATED: Use shell() or launchApplication() for new code
    returns:
    success: pid
    failure: None
    """

  @abstractmethod
  def launchProcess(self, cmd, outputFile = "process.txt", cwd = '', env = '', failIfRunning=False):
    """
    external function
    DEPRECATED: Use shell() or launchApplication() for new code
    returns:
    success: output filename
    failure: None
    """

  def processExist(self, appname):
    """
    iterates process list and returns pid if exists, otherwise None
    external function
    returns:
    success: pid
    failure: None
    """
    
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

    procList = self.getProcessList()
    if (procList == []):
      return None
      
    for proc in procList:
      procName = proc[1].split('/')[-1]
      if (procName == app):
        pid = proc[0]
        break
    return pid


  @abstractmethod
  def killProcess(self, appname, forceKill=False):
    """
    external function
    returns:
    success: True
    failure: False
    """
    
  @abstractmethod
  def catFile(self, remoteFile):
    """
    external function
    returns:
    success: filecontents
    failure: None
    """
    
  @abstractmethod
  def pullFile(self, remoteFile):
    """
    external function
    returns:
    success: output of pullfile, string
    failure: None
    """
    
  @abstractmethod
  def getFile(self, remoteFile, localFile = ''):
    """
    copy file from device (remoteFile) to host (localFile)
    external function
    returns:
    success: output of pullfile, string
    failure: None
    """
    
  @abstractmethod
  def getDirectory(self, remoteDir, localDir, checkDir=True):
    """
    copy directory structure from device (remoteDir) to host (localDir)
    external function
    checkDir exists so that we don't create local directories if the
    remote directory doesn't exist but also so that we don't call isDir
    twice when recursing.
    returns:
    success: list of files, string
    failure: None
    """
    
  @abstractmethod
  def isDir(self, remotePath):
    """
    external function
    returns:
    success: True
    failure: False
    Throws a FileError exception when null (invalid dir/filename)
    """
    
  @abstractmethod
  def validateFile(self, remoteFile, localFile):
    """
    true/false check if the two files have the same md5 sum
    external function
    returns:
    success: True
    failure: False
    """
    
  @abstractmethod
  def getRemoteHash(self, filename):
    """
    return the md5 sum of a remote file
    internal function
    returns:
    success: MD5 hash for given filename
    failure: None
    """
    
  def getLocalHash(self, filename):
    """
    return the md5 sum of a file on the host
    internal function
    returns:
    success: MD5 hash for given filename
    failure: None
    """
    
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

  @abstractmethod
  def getDeviceRoot(self):
    """
    Gets the device root for the testing area on the device
    For all devices we will use / type slashes and depend on the device-agent
    to sort those out.  The agent will return us the device location where we
    should store things, we will then create our /tests structure relative to
    that returned path.
    Structure on the device is as follows:
    /tests
          /<fennec>|<firefox>  --> approot
          /profile
          /xpcshell
          /reftest
          /mochitest
    external
    returns:
    success: path for device root
    failure: None
    """

  @abstractmethod
  def getAppRoot(self):
    """
    Either we will have /tests/fennec or /tests/firefox but we will never have
    both.  Return the one that exists
    TODO: ensure we can support org.mozilla.firefox
    external function
    returns:
    success: path for app root
    failure: None
    """

  def getTestRoot(self, type):
    """
    Gets the directory location on the device for a specific test type
    Type is one of: xpcshell|reftest|mochitest
    external function
    returns:
    success: path for test root
    failure: None
    """
    
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

  def signal(self, processID, signalType, signalAction):
    """
    Sends a specific process ID a signal code and action.
    For Example: SIGINT and SIGDFL to process x
    """
    #currently not implemented in device agent - todo
    
    pass

  def getReturnCode(self, processID):
    """Get a return code from process ending -- needs support on device-agent"""
    # TODO: make this real
    
    return 0

  @abstractmethod
  def unpackFile(self, filename):
    """
    external function
    returns:
    success: output of unzip command
    failure: None
    """
    
  @abstractmethod
  def reboot(self, ipAddr=None, port=30000):
    """
    external function
    returns:
    success: status from test agent
    failure: None
    """
    
  def validateDir(self, localDir, remoteDir):
    """
    validate localDir from host to remoteDir on the device
    external function
    returns:
    success: True
    failure: False
    """
    
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
    
  @abstractmethod
  def getInfo(self, directive=None):
    """
    Returns information about the device:
    Directive indicates the information you want to get, your choices are:
    os - name of the os
    id - unique id of the device
    uptime - uptime of the device
    systime - system time of the device
    screen - screen resolution
    memory - memory stats
    process - list of running processes (same as ps)
    disk - total, free, available bytes on disk
    power - power status (charge, battery temp)
    all - all of them - or call it with no parameters to get all the information
    returns:
    success: dict of info strings by directive name
    failure: None
    """
    
  @abstractmethod
  def installApp(self, appBundlePath, destPath=None):
    """
    external function
    returns:
    success: output from agent for inst command
    failure: None
    """

  @abstractmethod
  def uninstallAppAndReboot(self, appName, installPath=None):
    """
    external function
    returns:
    success: True
    failure: None
    """
    
  @abstractmethod
  def updateApp(self, appBundlePath, processName=None,
                destPath=None, ipAddr=None, port=30000):
    """
    external function
    returns:
    success: text status from command or callback server
    failure: None
    """
  
  @abstractmethod
  def getCurrentTime(self):
    """
    external function
    returns:
    success: time in ms
    failure: None
    """

  def recordLogcat(self):
    """
    external function
    returns:
    success: file is created in <testroot>/logcat.log
    failure: 
    """
    #TODO: spawn this off in a separate thread/process so we can collect all the logcat information

    # Right now this is just clearing the logcat so we can only see what happens after this call.
    buf = StringIO.StringIO()
    self.shell(['/system/bin/logcat', '-c'], buf)

  def getLogcat(self):
    """
    external function
    returns: data from the local file
    success: file is in 'filename'
    failure: None
    """
    buf = StringIO.StringIO()
    if self.shell(["/system/bin/logcat", "-d", "dalvikvm:S", "ConnectivityService:S", "WifiMonitor:S", "WifiStateTracker:S", "wpa_supplicant:S", "NetworkStateTracker:S"], buf) != 0:
      return None

    return str(buf.getvalue()[0:-1]).rstrip().split('\r')

  @abstractmethod
  def chmodDir(self, remoteDir):
    """
    external function
    returns:
    success: True
    failure: False
    """
    
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

def _pop_last_line(file):
  '''
  Utility function to get the last line from a file (shared between ADB and
  SUT device managers). Function also removes it from the file. Intended to
  strip off the return code from a shell command.
  '''
  bytes_from_end = 1
  file.seek(0, 2)
  length = file.tell() + 1
  while bytes_from_end < length:
    file.seek((-1)*bytes_from_end, 2)
    data = file.read()

    if bytes_from_end == length-1 and len(data) == 0: # no data, return None
      return None

    if data[0] == '\n' or bytes_from_end == length-1:
      # found the last line, which should have the return value
      if data[0] == '\n':
        data = data[1:]

      # truncate off the return code line
      file.truncate(length - bytes_from_end)
      file.seek(0,2)
      file.write('\0')

      return data

    bytes_from_end += 1

  return None
