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

import socket
import time, datetime
import os
import re
import hashlib
import subprocess
from threading import Thread
import traceback
import sys

class FileError(Exception):
  " Signifies an error which occurs while doing a file operation."

  def __init__(self, msg = ''):
    self.msg = msg

  def __str__(self):
    return self.msg

class DeviceManager:
  host = ''
  port = 0
  debug = 2
  _redo = False
  deviceRoot = None
  tempRoot = os.getcwd()
  base_prompt = '\$\>'
  prompt_sep = '\x00'
  prompt_regex = '.*' + base_prompt + prompt_sep
  agentErrorRE = re.compile('^##AGENT-ERROR##.*')

  def __init__(self, host, port = 20701):
    self.host = host
    self.port = port
    self._sock = None
    self.getDeviceRoot()

  def cmdNeedsResponse(self, cmd):
    """ Not all commands need a response from the agent:
        * if the cmd matches the pushRE then it is the first half of push
          and therefore we want to wait until the second half before looking
          for a response
        * rebt obviously doesn't get a response
        * uninstall performs a reboot to ensure starting in a clean state and
          so also doesn't look for a response
    """
    noResponseCmds = [re.compile('^push .*$'),
                      re.compile('^rebt'),
                      re.compile('^uninst .*$')]

    for c in noResponseCmds:
      if (c.match(cmd)):
        return False
    
    # If the command is not in our list, then it gets a response
    return True

  def shouldCmdCloseSocket(self, cmd):
    """ Some commands need to close the socket after they are sent:
    * push
    * rebt
    * uninst
    * quit
    """
    
    socketClosingCmds = [re.compile('^push .*$'),
                         re.compile('^quit.*'),
                         re.compile('^rebt.*'),
                         re.compile('^uninst .*$')]

    for c in socketClosingCmds:
      if (c.match(cmd)):
        return True

    return False

  def sendCMD(self, cmdline, newline = True):
    promptre = re.compile(self.prompt_regex + '$')
    data = ""
    shouldCloseSocket = False
    recvGuard = 1000

    if (self._sock == None):
      try:
        if (self.debug >= 1):
          print "reconnecting socket"
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
      except:
        self._redo = True
        self._sock = None
        if (self.debug >= 2):
          print "unable to create socket"
        return None
      
      try:
        self._sock.connect((self.host, int(self.port)))
        self._sock.recv(1024)
      except:
        self._redo = True
        self._sock.close()
        self._sock = None
        if (self.debug >= 2):
          print "unable to connect socket"
        return None
    
    for cmd in cmdline:
      if newline: cmd += '\r\n'
      
      try:
        numbytes = self._sock.send(cmd)
        if (numbytes != len(cmd)):
          print "ERROR: our cmd was " + str(len(cmd)) + " bytes and we only sent " + str(numbytes)
          return None
        if (self.debug >= 4): print "send cmd: " + str(cmd)
      except:
        self._redo = True
        self._sock.close()
        self._sock = None
        return None
      
      # Check if the command should close the socket
      shouldCloseSocket = self.shouldCmdCloseSocket(cmd)

      # Handle responses from commands
      if (self.cmdNeedsResponse(cmd)):
        found = False
        loopguard = 0
        # TODO: We had an old sleep here but we don't need it

        while (found == False and (loopguard < recvGuard)):
          if (self.debug >= 4): print "recv'ing..."

          # Get our response
          try:
            temp = self._sock.recv(1024)
            if (self.debug >= 4): print "response: " + str(temp)
          except:
            self._redo = True
            self._sock.close()
            self._sock = None
            return None

          # If something goes wrong in the agent it will send back a string that
          # starts with '##AGENT-ERROR##'
          if (self.agentErrorRE.match(temp)):
            data = temp
            break

          lines = temp.split('\n')

          for line in lines:
            if (promptre.match(line)):
              found = True
          data += temp

          # If we violently lose the connection to the device, this loop tends to spin,
          # this guard prevents that
          loopguard = loopguard + 1

    # TODO: We had an old sleep here but we don't need it
    if (shouldCloseSocket == True):
      try:
        self._sock.close()
        self._sock = None
      except:
        self._redo = True
        self._sock = None
        return None

    return data
  
  
  # take a data blob and strip instances of the prompt '$>\x00'
  def stripPrompt(self, data):
    promptre = re.compile(self.prompt_regex + '.*')
    retVal = []
    lines = data.split('\n')
    for line in lines:
      try:
        while (promptre.match(line)):
          pieces = line.split(self.prompt_sep)
          index = pieces.index('$>')
          pieces.pop(index)
          line = self.prompt_sep.join(pieces)
      except(ValueError):
        pass
      retVal.append(line)

    return '\n'.join(retVal)
  

  def pushFile(self, localname, destname):
    if (self.debug >= 3): print "in push file with: " + localname + ", and: " + destname
    if (self.validateFile(destname, localname) == True):
      if (self.debug >= 3): print "files are validated"
      return ''

    if self.mkDirs(destname) == None:
      print "unable to make dirs: " + destname
      return None

    if (self.debug >= 3): print "sending: push " + destname
    
    filesize = os.path.getsize(localname)
    f = open(localname, 'rb')
    data = f.read()
    f.close()
    retVal = self.sendCMD(['push ' + destname + ' ' + str(filesize) + '\r\n', data], newline = False)
    
    if (self.debug >= 3): print "push returned: " + str(retVal)

    validated = False
    if (retVal):
      retline = self.stripPrompt(retVal).strip() 
      if (retline == None or self.agentErrorRE.match(retVal)):
        # Then we failed to get back a hash from agent, try manual validation
        validated = self.validateFile(destname, localname)
      else:
        # Then we obtained a hash from push
        localHash = self.getLocalHash(localname)
        if (str(localHash) == str(retline)):
          validated = True
    else:
      # We got nothing back from sendCMD, try manual validation
      validated = self.validateFile(destname, localname)

    if (validated):
      if (self.debug >= 3): print "Push File Validated!"
      return True
    else:
      if (self.debug >= 2): print "Push File Failed to Validate!"
      return None
  
  def mkDir(self, name):
    return self.sendCMD(['mkdr ' + name])
  
  # make directory structure on the device
  def mkDirs(self, filename):
    parts = filename.split('/')
    name = ""
    for part in parts:
      if (part == parts[-1]): break
      if (part != ""):
        name += '/' + part
        if (self.mkDir(name) == None):
          print "failed making directory: " + str(name)
          return None
    return ''

  # push localDir from host to remoteDir on the device
  def pushDir(self, localDir, remoteDir):
    if (self.debug >= 2): print "pushing directory: " + localDir + " to " + remoteDir
    for root, dirs, files in os.walk(localDir):
      parts = root.split(localDir)
      for file in files:
        remoteRoot = remoteDir + '/' + parts[1]
        remoteName = remoteRoot + '/' + file
        if (parts[1] == ""): remoteRoot = remoteDir
        if (self.pushFile(os.path.join(root, file), remoteName) == None):
          self.removeFile(remoteName)
          if (self.pushFile(os.path.join(root, file), remoteName) == None):
            return None
    return True

  def dirExists(self, dirname):
    match = ".*" + dirname + "$"
    dirre = re.compile(match)
    data = self.sendCMD(['cd ' + dirname, 'cwd'])
    if (data == None):
      return None
    retVal = self.stripPrompt(data)
    data = retVal.split('\n')
    found = False
    for d in data:
      if (dirre.match(d)): 
        found = True

    return found

  # Because we always have / style paths we make this a lot easier with some
  # assumptions
  def fileExists(self, filepath):
    s = filepath.split('/')
    containingpath = '/'.join(s[:-1])
    listfiles = self.listFiles(containingpath)
    for f in listfiles:
      if (f == s[-1]):
        return True
    return False

  # list files on the device, requires cd to directory first
  def listFiles(self, rootdir):
    if (self.dirExists(rootdir) == False):
      return []  
    data = self.sendCMD(['cd ' + rootdir, 'ls'])
    if (data == None):
      return None
    retVal = self.stripPrompt(data)
    return retVal.split('\n')

  def removeFile(self, filename):
    if (self.debug>= 2): print "removing file: " + filename
    return self.sendCMD(['rm ' + filename])
    
  # does a recursive delete of directory on the device: rm -Rf remoteDir
  def removeDir(self, remoteDir):
    self.sendCMD(['rmdr ' + remoteDir])

  def getProcessList(self):
    data = self.sendCMD(['ps'])
    if (data == None):
      return None
      
    retVal = self.stripPrompt(data)
    lines = retVal.split('\n')
    files = []
    for line in lines:
      if (line.strip() != ''):
        pidproc = line.strip().split()
        if (len(pidproc) == 2):
          files += [[pidproc[0], pidproc[1]]]
        elif (len(pidproc) == 3):
          #android returns <userID> <procID> <procName>
          files += [[pidproc[1], pidproc[2], pidproc[0]]]     
    return files

  def getMemInfo(self):
    data = self.sendCMD(['mems'])
    if (data == None):
      return None
    retVal = self.stripPrompt(data)
    # TODO: this is hardcoded for now
    fhandle = open("memlog.txt", 'a')
    fhandle.write("\n")
    fhandle.write(retVal)
    fhandle.close()

  def fireProcess(self, appname):
    if (self.debug >= 2): print "FIRE PROC: '" + appname + "'"
    
    if (self.processExist(appname) != ''):
      print "WARNING: process %s appears to be running already\n" % appname
    
    self.sendCMD(['exec ' + appname])

    #NOTE: we sleep for 30 seconds to allow the application to startup
    time.sleep(30)

    self.process = self.processExist(appname)
    if (self.debug >= 4): print "got pid: " + str(self.process) + " for process: " + str(appname)

  def launchProcess(self, cmd, outputFile = "process.txt", cwd = ''):
    cmdline = subprocess.list2cmdline(cmd)
    if (outputFile == "process.txt" or outputFile == None):
      outputFile = self.getDeviceRoot() + '/' + "process.txt"
      cmdline += " > " + outputFile

    self.fireProcess(cmdline)
    return outputFile
  
  #hardcoded: sleep interval of 5 seconds, timeout of 10 minutes
  def communicate(self, process, timeout = 600):
    interval = 5
    timed_out = True
    if (timeout > 0):
      total_time = 0
      while total_time < timeout:
        time.sleep(interval)
        if (not self.poll(process)):
          timed_out = False
          break
        total_time += interval

    if (timed_out == True):
      return None

    return [self.getFile(process, "temp.txt"), None]


  def poll(self, process):
    try:
      if (self.processExist(process) == None):
        return None
      return 1
    except:
      return None
    return 1
  
  # iterates process list and returns pid if exists, otherwise ''
  def processExist(self, appname):
    pid = ''
  
    pieces = appname.split(' ')
    parts = pieces[0].split('/')
    app = parts[-1]
    procre = re.compile('.*' + app + '.*')

    procList = self.getProcessList()
    if (procList == None):
      return None
      
    for proc in procList:
      if (procre.match(proc[1])):
        pid = proc[0]
        break
    return pid

  def killProcess(self, appname):
    if (self.sendCMD(['kill ' + appname]) == None):
      return None

    return True

  def getTempDir(self):
    retVal = ''
    data = self.sendCMD(['tmpd'])
    if (data == None):
      return None
    return self.stripPrompt(data).strip('\n')
  
  # copy file from device (remoteFile) to host (localFile)
  def getFile(self, remoteFile, localFile = ''):
    if localFile == '':
        localFile = os.path.join(self.tempRoot, "temp.txt")
  
    promptre = re.compile(self.prompt_regex + '.*')
    data = self.sendCMD(['cat ' + remoteFile])
    if (data == None):
      return None
    retVal = self.stripPrompt(data)
    fhandle = open(localFile, 'wb')
    fhandle.write(retVal)
    fhandle.close()
    return retVal
    
  # copy directory structure from device (remoteDir) to host (localDir)
  def getDirectory(self, remoteDir, localDir):
    if (self.debug >= 2): print "getting files in '" + remoteDir + "'"
    filelist = self.listFiles(remoteDir)
    if (filelist == None):
      return None
    if (self.debug >= 3): print filelist
    if not os.path.exists(localDir):
      os.makedirs(localDir)
  
    # TODO: is this a comprehensive file regex?
    isFile = re.compile('^([a-zA-Z0-9_\-\. ]+)\.([a-zA-Z0-9]+)$')
    for f in filelist:
      if (isFile.match(f)):
        if (self.getFile(remoteDir + '/' + f, os.path.join(localDir, f)) == None):
          return None
      else:
        if (self.getDirectory(remoteDir + '/' + f, os.path.join(localDir, f)) == None):
          return None

  # true/false check if the two files have the same md5 sum
  def validateFile(self, remoteFile, localFile):
    remoteHash = self.getRemoteHash(remoteFile)
    localHash = self.getLocalHash(localFile)

    if (remoteHash == localHash):
        return True

    return False
  
  # return the md5 sum of a remote file
  def getRemoteHash(self, filename):
      data = self.sendCMD(['hash ' + filename])
      if (data == None):
          return ''
      retVal = self.stripPrompt(data)
      if (retVal != None):
        retVal = retVal.strip('\n')
      if (self.debug >= 3): print "remote hash returned: '" + retVal + "'"
      return retVal
    
  # return the md5 sum of a file on the host
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
  def getDeviceRoot(self):
    if (not self.deviceRoot):
      data = self.sendCMD(['testroot'])
      if (data == None):
        return '/tests'
      self.deviceRoot = self.stripPrompt(data).strip('\n') + '/tests'

    if (not self.dirExists(self.deviceRoot)):
      self.mkDir(self.deviceRoot)

    return self.deviceRoot

  # Either we will have /tests/fennec or /tests/firefox but we will never have
  # both.  Return the one that exists
  def getAppRoot(self):
    if (self.dirExists(self.getDeviceRoot() + '/fennec')):
      return self.getDeviceRoot() + '/fennec'
    elif (self.dirExists(self.getDeviceRoot() + '/firefox')):
      return self.getDeviceRoot() + '/firefox'
    else:
      return 'org.mozilla.fennec'

  # Gets the directory location on the device for a specific test type
  # Type is one of: xpcshell|reftest|mochitest
  def getTestRoot(self, type):
    if (re.search('xpcshell', type, re.I)):
      self.testRoot = self.getDeviceRoot() + '/xpcshell'
    elif (re.search('?(i)reftest', type)):
      self.testRoot = self.getDeviceRoot() + '/reftest'
    elif (re.search('?(i)mochitest', type)):
      self.testRoot = self.getDeviceRoot() + '/mochitest'
    return self.testRoot

  # Sends a specific process ID a signal code and action.
  # For Example: SIGINT and SIGDFL to process x
  def signal(self, processID, signalType, signalAction):
    # currently not implemented in device agent - todo
    pass

  # Get a return code from process ending -- needs support on device-agent
  # this is a todo
  def getReturnCode(self, processID):
    # todo make this real
    return 0

  def unpackFile(self, filename):
    dir = ''
    parts = filename.split('/')
    if (len(parts) > 1):
      if self.fileExists(filename):
        dir = '/'.join(parts[:-1])
    elif self.fileExists('/' + filename):
      dir = '/' + filename
    elif self.fileExists(self.getDeviceRoot() + '/' + filename):
      dir = self.getDeviceRoot() + '/' + filename
    else:
      return None

    return self.sendCMD(['cd ' + dir, 'unzp ' + filename])

  def reboot(self, wait = False):
    self.sendCMD(['rebt'])

    if wait == True:
      time.sleep(30)
      timeout = 270
      done = False
      while (not done):
        if self.listFiles('/') != None:
          return ''
        print "sleeping another 10 seconds"
        time.sleep(10)
        timeout = timeout - 10
        if (timeout <= 0):
          return None
    return ''

  # validate localDir from host to remoteDir on the device
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
            return None
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
  def getInfo(self, directive=None):
    data = None
    result = {}
    collapseSpaces = re.compile('  +')

    directives = ['os', 'id','uptime','systime','screen','memory','process',
                  'disk','power']
    if (directive in directives):
      directives = [directive]

    for d in directives:
      data = self.sendCMD(['info ' + d])
      if (data is None):
        continue
      data = self.stripPrompt(data)
      data = collapseSpaces.sub(' ', data)
      result[d] = data.split('\n')

    # Get rid of any 0 length members of the arrays
    for v in result.itervalues():
      while '' in v:
        v.remove('')
    
    # Format the process output
    if 'process' in result:
      proclist = []
      for l in result['process']:
        if l:
          proclist.append(l.split('\t'))
      result['process'] = proclist

    print "results: " + str(result)
    return result

  """
  Installs the application onto the device
  Application bundle - path to the application bundle on the device
  Destination - destination directory of where application should be
                installed to (optional)
  Returns True or False depending on what we get back
  TODO: we need a real way to know if this works or not
  """
  def installApp(self, appBundlePath, destPath=None):
    cmd = 'inst ' + appBundlePath
    if destPath:
      cmd += ' ' + destPath
    data = self.sendCMD([cmd])
    if (data is None):
      return False
    else:
      return True

  """
  Uninstalls the named application from device and causes a reboot.
  Takes an optional argument of installation path - the path to where the application
  was installed.
  Returns True, but it doesn't mean anything other than the command was sent,
  the reboot happens and we don't know if this succeeds or not.
  """
  def uninstallAppAndReboot(self, appName, installPath=None):
    cmd = 'uninst ' + appName
    if installPath:
      cmd += ' ' + installPath
    self.sendCMD([cmd])
    return True

  """
    return the current time on the device
  """
  def getCurrentTime(self):
    data = self.sendCMD(['clok'])
    if (data == None):
      return None
    return self.stripPrompt(data).strip('\n')

