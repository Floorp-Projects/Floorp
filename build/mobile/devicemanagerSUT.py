# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import socket
import SocketServer
import time, datetime
import os
import re
import hashlib
import subprocess
from threading import Thread
import traceback
import sys
import StringIO
from devicemanager import DeviceManager, DMError, FileError, NetworkTools, _pop_last_line

class AgentError(Exception):
  "SUTAgent-specific exception."

  def __init__(self, msg= '', fatal = False):
    self.msg = msg
    self.fatal = fatal

  def __str__(self):
    return self.msg

class DeviceManagerSUT(DeviceManager):
  host = ''
  port = 0
  debug = 2 
  retries = 0
  tempRoot = os.getcwd()
  base_prompt = '$>'
  base_prompt_re = '\$\>'
  prompt_sep = '\x00'
  prompt_regex = '.*(' + base_prompt_re + prompt_sep + ')'
  agentErrorRE = re.compile('^##AGENT-WARNING##.*')

  # TODO: member variable to indicate error conditions.
  # This should be set to a standard error from the errno module.
  # So, for example, when an error occurs because of a missing file/directory,
  # before returning, the function would do something like 'self.error = errno.ENOENT'.
  # The error would be set where appropriate--so sendCMD() could set socket errors,
  # pushFile() and other file-related commands could set filesystem errors, etc.

  def __init__(self, host, port = 20701, retrylimit = 5):
    self.host = host
    self.port = port
    self.retrylimit = retrylimit
    self.retries = 0
    self._sock = None
    if self.getDeviceRoot() == None:
        raise BaseException("Failed to connect to SUT Agent and retrieve the device root.")

  def _cmdNeedsResponse(self, cmd):
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
                      re.compile('^uninst .*$'),
                      re.compile('^pull .*$')]

    for c in noResponseCmds:
      if (c.match(cmd)):
        return False

    # If the command is not in our list, then it gets a response
    return True

  def _stripPrompt(self, data):
    '''
    internal function
    take a data blob and strip instances of the prompt '$>\x00'
    '''
    promptre = re.compile(self.prompt_regex + '.*')
    retVal = []
    lines = data.split('\n')
    for line in lines:
      foundPrompt = False
      try:
        while (promptre.match(line)):
          foundPrompt = True
          pieces = line.split(self.prompt_sep)
          index = pieces.index('$>')
          pieces.pop(index)
          line = self.prompt_sep.join(pieces)
      except(ValueError):
        pass

      # we don't want to append lines that are blank after stripping the
      # prompt (those are basically "prompts")
      if not foundPrompt or line:
        retVal.append(line)

    return '\n'.join(retVal)

  def _shouldCmdCloseSocket(self, cmd):
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

  def sendCmds(self, cmdlist, outputfile, timeout = None, newline = True):
    '''
    a wrapper for _doCmds that loops up to self.retrylimit iterations.
    this allows us to move the retry logic outside of the _doCmds() to make it
    easier for debugging in the future.
    note that since cmdlist is a list of commands, they will all be retried if
    one fails.  this is necessary in particular for pushFile(), where we don't want
    to accidentally send extra data if a failure occurs during data transmission.
    '''
    done = False
    while self.retries < self.retrylimit:
      try:
        self._doCmds(cmdlist, outputfile, timeout, newline)
        return
      except AgentError, err:
        # re-raise error if it's fatal (i.e. the device got the command but
        # couldn't execute it). retry otherwise
        if err.fatal:
          raise err
        if self.debug >= 2:
          print err
        self.retries += 1

    raise AgentError("unable to connect to %s after %s attempts" % (self.host, self.retrylimit))

  def runCmds(self, cmdlist, timeout = None, newline = True):
    '''
    similar to sendCmds, but just returns any output as a string instead of
    writing to a file. this is normally what you want to call to send a set
    of commands to the agent
    '''
    outputfile = StringIO.StringIO()
    self.sendCmds(cmdlist, outputfile, timeout, newline)
    outputfile.seek(0)
    return outputfile.read()

  def _doCmds(self, cmdlist, outputfile, timeout, newline):
    promptre = re.compile(self.prompt_regex + '$')
    shouldCloseSocket = False
    recvGuard = 1000

    if not self._sock:
      try:
        if self.debug >= 1:
          print "reconnecting socket"
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
      except socket.error, msg:
        self._sock = None
        raise AgentError("unable to create socket: "+str(msg))

      try:
        self._sock.connect((self.host, int(self.port)))
        self._sock.recv(1024)
      except socket.error, msg:
        self._sock.close()
        self._sock = None
        raise AgentError("unable to connect socket: "+str(msg))

    for cmd in cmdlist:
      if newline: cmd += '\r\n'

      try:
        numbytes = self._sock.send(cmd)
        if (numbytes != len(cmd)):
          raise AgentError("ERROR: our cmd was %s bytes and we only sent %s" % (len(cmd),
                                                                                numbytes))
        if (self.debug >= 4): print "send cmd: " + str(cmd)
      except socket.error, msg:
        self._sock.close()
        self._sock = None
        if self.debug >= 1:
          print "Error sending data to socket. cmd="+str(cmd)+"; err="+str(msg)
        return False

      # Check if the command should close the socket
      shouldCloseSocket = self._shouldCmdCloseSocket(cmd)

      # Handle responses from commands
      if (self._cmdNeedsResponse(cmd)):
        found = False
        loopguard = 0
        data = ""

        while (found == False and (loopguard < recvGuard)):
          temp = ''
          if (self.debug >= 4): print "recv'ing..."

          # Get our response
          try:
            temp = self._sock.recv(1024)
            if (self.debug >= 4): print "response: " + str(temp)
          except socket.error, msg:
            self._sock.close()
            self._sock = None
            raise AgentError("Error receiving data from socket. cmd="+str(cmd)+"; err="+str(msg))

          data += temp

          # If something goes wrong in the agent it will send back a string that
          # starts with '##AGENT-ERROR##'
          if self.agentErrorRE.match(data):
            raise AgentError("Agent Error processing command: %s" % cmd, fatal=True)

          for line in data.splitlines():
            if promptre.match(line):
              found = True
              data = self._stripPrompt(data)
              break

          # periodically flush data to output file to make sure it doesn't get
          # too big/unwieldly
          if len(data) > 1024:
              outputfile.write(data[0:1024])
              data = data[1024:]

          # If we violently lose the connection to the device, this loop tends to spin,
          # this guard prevents that
          if (temp == ''):
            loopguard += 1

        # Write any remaining data to outputfile
        outputfile.write(data)

    if shouldCloseSocket:
      try:
        self._sock.close()
        self._sock = None
      except:
        self._sock = None
        raise AgentError("Error closing socket")

  # external function: executes shell command on device
  # returns:
  # success: <return code>
  # failure: None
  def shell(self, cmd, outputfile, env=None, cwd=None):
    cmdline = subprocess.list2cmdline(cmd)
    if env:
      cmdline = '%s %s' % (self.formatEnvString(env), cmdline)

    try:
      if cwd:
        self.sendCmds(['execcwd %s %s' % (cwd, cmdline)], outputfile)
      else:
        self.sendCmds(['exec su -c "%s"' % cmdline], outputfile)
    except AgentError:
      return None

    # dig through the output to get the return code
    lastline = _pop_last_line(outputfile)
    if lastline:
      m = re.search('return code \[([0-9]+)\]', lastline)
      if m:
        return int(m.group(1))

    # woops, we couldn't find an end of line/return value
    return None

  # external function
  # returns:
  #  success: True
  #  failure: False
  def pushFile(self, localname, destname):
    if (os.name == "nt"):
      destname = destname.replace('\\', '/')

    if (self.debug >= 3): print "in push file with: " + localname + ", and: " + destname
    if (self.dirExists(destname)):
      if (not destname.endswith('/')):
        destname = destname + '/'
      destname = destname + os.path.basename(localname)
    if (self.validateFile(destname, localname) == True):
      if (self.debug >= 3): print "files are validated"
      return True

    if self.mkDirs(destname) == None:
      print "unable to make dirs: " + destname
      return False

    if (self.debug >= 3): print "sending: push " + destname
    
    filesize = os.path.getsize(localname)
    f = open(localname, 'rb')
    data = f.read()
    f.close()

    try:
      retVal = self.runCmds(['push ' + destname + ' ' + str(filesize) + '\r\n', data], newline = False)
    except AgentError:
      retVal = False
  
    if (self.debug >= 3): print "push returned: " + str(retVal)

    validated = False
    if (retVal):
      retline = retVal.strip()
      if (retline == None):
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
      return False
  
  # external function
  # returns:
  #  success: directory name
  #  failure: None
  def mkDir(self, name):
    if (self.dirExists(name)):
      return name
    else:
      try:
        retVal = self.runCmds(['mkdr ' + name])
      except AgentError:
        retVal = None
      return retVal

  # make directory structure on the device
  # external function
  # returns:
  #  success: directory structure that we created
  #  failure: None
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
    return name

  # push localDir from host to remoteDir on the device
  # external function
  # returns:
  #  success: remoteDir
  #  failure: None
  def pushDir(self, localDir, remoteDir):
    if (self.debug >= 2): print "pushing directory: %s to %s" % (localDir, remoteDir)
    for root, dirs, files in os.walk(localDir, followlinks=True):
      parts = root.split(localDir)
      for file in files:
        remoteRoot = remoteDir + '/' + parts[1]
        if (remoteRoot.endswith('/')):
          remoteName = remoteRoot + file
        else:
          remoteName = remoteRoot + '/' + file
        if (parts[1] == ""): remoteRoot = remoteDir
        if (self.pushFile(os.path.join(root, file), remoteName) == False):
          # retry once
          self.removeFile(remoteName)
          if (self.pushFile(os.path.join(root, file), remoteName) == False):
            return None
    return remoteDir

  # external function
  # returns:
  #  success: True
  #  failure: False
  def dirExists(self, dirname):
    match = ".*" + dirname.replace('^', '\^') + "$"
    dirre = re.compile(match)
    try:
      data = self.runCmds(['cd ' + dirname, 'cwd'])
    except AgentError:
      return False

    found = False
    for d in data.splitlines():
      if (dirre.match(d)):
        found = True

    return found

  # Because we always have / style paths we make this a lot easier with some
  # assumptions
  # external function
  # returns:
  #  success: True
  #  failure: False
  def fileExists(self, filepath):
    s = filepath.split('/')
    containingpath = '/'.join(s[:-1])
    listfiles = self.listFiles(containingpath)
    for f in listfiles:
      if (f == s[-1]):
        return True
    return False

  # list files on the device, requires cd to directory first
  # external function
  # returns:
  #  success: array of filenames, ['file1', 'file2', ...]
  #  failure: []
  def listFiles(self, rootdir):
    rootdir = rootdir.rstrip('/')
    if (self.dirExists(rootdir) == False):
      return []
    try:
      data = self.runCmds(['cd ' + rootdir, 'ls'])
    except AgentError:
      return []

    files = filter(lambda x: x, data.splitlines())
    if len(files) == 1 and files[0] == '<empty>':
      # special case on the agent: empty directories return just the string "<empty>"
      return []
    return files

  # external function
  # returns:
  #  success: output of telnet, i.e. "removing file: /mnt/sdcard/tests/test.txt"
  #  failure: None
  def removeFile(self, filename):
    if (self.debug>= 2): print "removing file: " + filename
    try:
      retVal = self.runCmds(['rm ' + filename])
    except AgentError:
      return None

    return retVal
  
  # does a recursive delete of directory on the device: rm -Rf remoteDir
  # external function
  # returns:
  #  success: output of telnet, i.e. "removing file: /mnt/sdcard/tests/test.txt"
  #  failure: None
  def removeDir(self, remoteDir):
    try:
      retVal = self.runCmds(['rmdr ' + remoteDir])
    except AgentError:
      return None

    return retVal

  # external function
  # returns:
  #  success: array of process tuples
  #  failure: []
  def getProcessList(self):
    try:
      data = self.runCmds(['ps'])
    except AgentError:
      return []

    files = []
    for line in data.splitlines():
      if line:
        pidproc = line.strip().split()
        if (len(pidproc) == 2):
          files += [[pidproc[0], pidproc[1]]]
        elif (len(pidproc) == 3):
          #android returns <userID> <procID> <procName>
          files += [[pidproc[1], pidproc[2], pidproc[0]]]     
    return files

  # external function
  # DEPRECATED: Use shell() or launchApplication() for new code
  # returns:
  #  success: pid
  #  failure: None
  def fireProcess(self, appname, failIfRunning=False):
    if (not appname):
      if (self.debug >= 1): print "WARNING: fireProcess called with no command to run"
      return None

    if (self.debug >= 2): print "FIRE PROC: '" + appname + "'"

    if (self.processExist(appname) != None):
      print "WARNING: process %s appears to be running already\n" % appname
      if (failIfRunning):
        return None
    
    try:
      data = self.runCmds(['exec ' + appname])
    except AgentError:
      return None

    # wait up to 30 seconds for process to start up
    timeslept = 0
    while (timeslept <= 30):
      process = self.processExist(appname)
      if (process is not None):
        break
      time.sleep(3)
      timeslept += 3

    if (self.debug >= 4): print "got pid: %s for process: %s" % (process, appname)
    return process

  # external function
  # DEPRECATED: Use shell() or launchApplication() for new code
  # returns:
  #  success: output filename
  #  failure: None
  def launchProcess(self, cmd, outputFile = "process.txt", cwd = '', env = '', failIfRunning=False):
    if not cmd:
      if (self.debug >= 1): print "WARNING: launchProcess called without command to run"
      return None

    cmdline = subprocess.list2cmdline(cmd)
    if (outputFile == "process.txt" or outputFile == None):
      outputFile = self.getDeviceRoot();
      if outputFile is None:
        return None
      outputFile += "/process.txt"
      cmdline += " > " + outputFile
    
    # Prepend our env to the command 
    cmdline = '%s %s' % (self.formatEnvString(env), cmdline)

    if self.fireProcess(cmdline, failIfRunning) is None:
      return None
    return outputFile

  # external function
  # returns:
  #  success: True
  #  failure: False
  def killProcess(self, appname, forceKill=False):
    if forceKill:
      print "WARNING: killProcess(): forceKill parameter unsupported on SUT"
    try:
      data = self.runCmds(['kill ' + appname])
    except AgentError:
      return False

    return True

  # external function
  # returns:
  #  success: tmpdir, string
  #  failure: None
  def getTempDir(self):
    try:
      data = self.runCmds(['tmpd'])
    except AgentError:
      return None

    return data.strip()

  # external function
  # returns:
  #  success: filecontents
  #  failure: None
  def catFile(self, remoteFile):
    try:
      data = self.runCmds(['cat ' + remoteFile])
    except AgentError:
      return None

    return data

  # external function
  # returns:
  #  success: output of pullfile, string
  #  failure: None
  def pullFile(self, remoteFile):
    """Returns contents of remoteFile using the "pull" command.
    The "pull" command is different from other commands in that DeviceManager
    has to read a certain number of bytes instead of just reading to the
    next prompt.  This is more robust than the "cat" command, which will be
    confused if the prompt string exists within the file being catted.
    However it means we can't use the response-handling logic in sendCMD().
    """
    
    def err(error_msg):
        err_str = 'error returned from pull: %s' % error_msg
        print err_str
        self._sock = None
        raise FileError(err_str) 

    # FIXME: We could possibly move these socket-reading functions up to
    # the class level if we wanted to refactor sendCMD().  For now they are
    # only used to pull files.
    
    def uread(to_recv, error_msg):
      """ unbuffered read """
      try:
        data = self._sock.recv(to_recv)
        if not data:
          err(error_msg)
          return None
        return data
      except:
        err(error_msg)
        return None

    def read_until_char(c, buffer, error_msg):
      """ read until 'c' is found; buffer rest """
      while not '\n' in buffer:
        data = uread(1024, error_msg)
        if data == None:
          err(error_msg)
          return ('', '', '')
        buffer += data
      return buffer.partition(c)

    def read_exact(total_to_recv, buffer, error_msg):
      """ read exact number of 'total_to_recv' bytes """
      while len(buffer) < total_to_recv:
        to_recv = min(total_to_recv - len(buffer), 1024)
        data = uread(to_recv, error_msg)
        if data == None:
          return None
        buffer += data
      return buffer

    prompt = self.base_prompt + self.prompt_sep
    buffer = ''
    
    # expected return value:
    # <filename>,<filesize>\n<filedata>
    # or, if error,
    # <filename>,-1\n<error message>
    try:
      data = self.runCmds(['pull ' + remoteFile])
    except AgentError:
      return None

    # read metadata; buffer the rest
    metadata, sep, buffer = read_until_char('\n', buffer, 'could not find metadata')
    if not metadata:
      return None
    if self.debug >= 3:
      print 'metadata: %s' % metadata

    filename, sep, filesizestr = metadata.partition(',')
    if sep == '':
      err('could not find file size in returned metadata')
      return None
    try:
        filesize = int(filesizestr)
    except ValueError:
      err('invalid file size in returned metadata')
      return None

    if filesize == -1:
      # read error message
      error_str, sep, buffer = read_until_char('\n', buffer, 'could not find error message')
      if not error_str:
        return None
      # prompt should follow
      read_exact(len(prompt), buffer, 'could not find prompt')
      print "DeviceManager: error pulling file '%s': %s" % (remoteFile, error_str)
      return None

    # read file data
    total_to_recv = filesize + len(prompt)
    buffer = read_exact(total_to_recv, buffer, 'could not get all file data')
    if buffer == None:
      return None
    if buffer[-len(prompt):] != prompt:
      err('no prompt found after file data--DeviceManager may be out of sync with agent')
      return buffer
    return buffer[:-len(prompt)]

  # copy file from device (remoteFile) to host (localFile)
  # external function
  # returns:
  #  success: output of pullfile, string
  #  failure: None
  def getFile(self, remoteFile, localFile = ''):
    if localFile == '':
      localFile = os.path.join(self.tempRoot, "temp.txt")
  
    try:
      retVal = self.pullFile(remoteFile)
    except:
      return None
      
    if (retVal is None):
      return None

    fhandle = open(localFile, 'wb')
    fhandle.write(retVal)
    fhandle.close()
    if not self.validateFile(remoteFile, localFile):
      print 'failed to validate file when downloading %s!' % remoteFile
      return None
    return retVal

  # copy directory structure from device (remoteDir) to host (localDir)
  # external function
  # checkDir exists so that we don't create local directories if the
  # remote directory doesn't exist but also so that we don't call isDir
  # twice when recursing.
  # returns:
  #  success: list of files, string
  #  failure: None
  def getDirectory(self, remoteDir, localDir, checkDir=True):
    if (self.debug >= 2): print "getting files in '" + remoteDir + "'"
    if checkDir:
      try:
        is_dir = self.isDir(remoteDir)
      except FileError:
        return None
      if not is_dir:
        return None
        
    filelist = self.listFiles(remoteDir)
    if (self.debug >= 3): print filelist
    if not os.path.exists(localDir):
      os.makedirs(localDir)

    for f in filelist:
      if f == '.' or f == '..':
        continue
      remotePath = remoteDir + '/' + f
      localPath = os.path.join(localDir, f)
      try:
        is_dir = self.isDir(remotePath)
      except FileError:
        print 'isdir failed on file "%s"; continuing anyway...' % remotePath
        continue
      if is_dir:
        if (self.getDirectory(remotePath, localPath, False) == None):
          print 'failed to get directory "%s"' % remotePath
          return None
      else:
        # It's sometimes acceptable to have getFile() return None, such as
        # when the agent encounters broken symlinks.
        # FIXME: This should be improved so we know when a file transfer really
        # failed.
        if self.getFile(remotePath, localPath) == None:
          print 'failed to get file "%s"; continuing anyway...' % remotePath 
    return filelist

  # external function
  # returns:
  #  success: True
  #  failure: False
  #  Throws a FileError exception when null (invalid dir/filename)
  def isDir(self, remotePath):
    try:
      data = self.runCmds(['isdir ' + remotePath])
    except AgentError:
      # normally there should be no error here; a nonexistent file/directory will
      # return the string "<filename>: No such file or directory".
      # However, I've seen AGENT-WARNING returned before. 
      return False

    retVal = data.strip()
    if not retVal:
      raise FileError('isdir returned null')
    return retVal == 'TRUE'

  # true/false check if the two files have the same md5 sum
  # external function
  # returns:
  #  success: True
  #  failure: False
  def validateFile(self, remoteFile, localFile):
    remoteHash = self.getRemoteHash(remoteFile)
    localHash = self.getLocalHash(localFile)

    if (remoteHash == None):
      return False

    if (remoteHash == localHash):
      return True

    return False
  
  # return the md5 sum of a remote file
  # internal function
  # returns:
  #  success: MD5 hash for given filename
  #  failure: None
  def getRemoteHash(self, filename):
    try:
      data = self.runCmds(['hash ' + filename])
    except AgentError:
      return None

    retVal = None
    if data:
      retVal = data.strip()
    if (self.debug >= 3): print "remote hash returned: '" + retVal + "'"
    return retVal
    
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
    try:
      data = self.runCmds(['testroot'])
    except:
      return None

    deviceRoot = data.strip() + '/tests'

    if (not self.dirExists(deviceRoot)):
      if (self.mkDir(deviceRoot) == None):
        return None

    return deviceRoot

  def getAppRoot(self, packageName):
    try:
      data = self.runCmds(['getapproot '+packageName])
    except:
      return None

    return data.strip()

  # external function
  # returns:
  #  success: output of unzip command
  #  failure: None
  def unpackFile(self, filename):
    devroot = self.getDeviceRoot()
    if (devroot == None):
      return None

    dir = ''
    parts = filename.split('/')
    if (len(parts) > 1):
      if self.fileExists(filename):
        dir = '/'.join(parts[:-1])
    elif self.fileExists('/' + filename):
      dir = '/' + filename
    elif self.fileExists(devroot + '/' + filename):
      dir = devroot + '/' + filename
    else:
      return None

    try:
      data = self.runCmds(['cd ' + dir, 'unzp ' + filename])
    except AgentError:
      return None

    return data

  # external function
  # returns:
  #  success: status from test agent
  #  failure: None
  def reboot(self, ipAddr=None, port=30000):
    cmd = 'rebt'   

    if (self.debug > 3): print "INFO: sending rebt command"
    callbacksvrstatus = None    

    if (ipAddr is not None):
    #create update.info file:
      try:
        destname = '/data/data/com.mozilla.SUTAgentAndroid/files/update.info'
        data = "%s,%s\rrebooting\r" % (ipAddr, port)
        self.runCmds(['push ' + destname + ' ' + str(len(data)) + '\r\n', data], newline = False)
      except AgentError:
        return None

      ip, port = self.getCallbackIpAndPort(ipAddr, port)
      cmd += " %s %s" % (ip, port)
      # Set up our callback server
      callbacksvr = callbackServer(ip, port, self.debug)

    try:
      status = self.runCmds([cmd])
    except AgentError:
      return None

    if (ipAddr is not None):
      status = callbacksvr.disconnect()

    if (self.debug > 3): print "INFO: rebt- got status back: " + str(status)
    return status

  # Returns information about the device:
  # Directive indicates the information you want to get, your choices are:
  # os - name of the os
  # id - unique id of the device
  # uptime - uptime of the device
  # systime - system time of the device
  # screen - screen resolution
  # rotation - rotation of the device (in degrees)
  # memory - memory stats
  # process - list of running processes (same as ps)
  # disk - total, free, available bytes on disk
  # power - power status (charge, battery temp)
  # all - all of them - or call it with no parameters to get all the information
  # returns:
  #   success: dict of info strings by directive name
  #   failure: {}
  def getInfo(self, directive=None):
    data = None
    result = {}
    collapseSpaces = re.compile('  +')

    directives = ['os','id','uptime','systime','screen','rotation','memory','process',
                  'disk','power']
    if (directive in directives):
      directives = [directive]

    for d in directives:
      data = self.runCmds(['info ' + d])
      if (data is None):
        continue
      data = collapseSpaces.sub(' ', data)
      result[d] = data.split('\n')

    # Get rid of any 0 length members of the arrays
    for k, v in result.iteritems():
      result[k] = filter(lambda x: x != '', result[k])
    
    # Format the process output
    if 'process' in result:
      proclist = []
      for l in result['process']:
        if l:
          proclist.append(l.split('\t'))
      result['process'] = proclist

    if (self.debug >= 3): print "results: " + str(result)
    return result

  """
  Installs the application onto the device
  Application bundle - path to the application bundle on the device
  Destination - destination directory of where application should be
                installed to (optional)
  Returns None for success, or output if known failure
  """
  # external function
  # returns:
  #  success: output from agent for inst command
  #  failure: None
  def installApp(self, appBundlePath, destPath=None):
    cmd = 'inst ' + appBundlePath
    if destPath:
      cmd += ' ' + destPath
    try:
      data = self.runCmds([cmd])
    except AgentError:
      return None

    f = re.compile('Failure')
    for line in data.split():
      if (f.match(line)):
        return data
    return None

  """
  Uninstalls the named application from device and causes a reboot.
  Takes an optional argument of installation path - the path to where the application
  was installed.
  Returns True, but it doesn't mean anything other than the command was sent,
  the reboot happens and we don't know if this succeeds or not.
  """
  # external function
  # returns:
  #  success: True
  #  failure: None
  def uninstallAppAndReboot(self, appName, installPath=None):
    cmd = 'uninst ' + appName
    if installPath:
      cmd += ' ' + installPath
    try:
      data = self.runCmds([cmd])
    except AgentError:
      return None

    if (self.debug > 3): print "uninstallAppAndReboot: " + str(data)
    return True

  """
  Updates the application on the device.
  Application bundle - path to the application bundle on the device
  Process name of application - used to end the process if the applicaiton is
                                currently running
  Destination - Destination directory to where the application should be
                installed (optional)
  ipAddr - IP address to await a callback ping to let us know that the device has updated
           properly - defaults to current IP.
  port - port to await a callback ping to let us know that the device has updated properly
         defaults to 30000, and counts up from there if it finds a conflict
  Returns True if succeeds, False if not
  """
  # external function
  # returns:
  #  success: text status from command or callback server
  #  failure: None
  def updateApp(self, appBundlePath, processName=None, destPath=None, ipAddr=None, port=30000):
    status = None
    cmd = 'updt '
    if (processName == None):
      # Then we pass '' for processName
      cmd += "'' " + appBundlePath
    else:
      cmd += processName + ' ' + appBundlePath

    if (destPath):
      cmd += " " + destPath

    if (ipAddr is not None):
      ip, port = self.getCallbackIpAndPort(ipAddr, port)
      cmd += " %s %s" % (ip, port)
      # Set up our callback server
      callbacksvr = callbackServer(ip, port, self.debug)

    if (self.debug >= 3): print "INFO: updateApp using command: " + str(cmd)

    try:
      status = self.runCmds([cmd])
    except AgentError:
      return None

    if ipAddr is not None:
      status = callbacksvr.disconnect()

    if (self.debug >= 3): print "INFO: updateApp: got status back: " + str(status)

    return status

  """
    return the current time on the device
  """
  # external function
  # returns:
  #  success: time in ms
  #  failure: None
  def getCurrentTime(self):
    try:
      data = self.runCmds(['clok'])
    except AgentError:
      return None

    return data.strip()

  """
    Connect the ipaddress and port for a callback ping.  Defaults to current IP address
    And ports starting at 30000.
    NOTE: the detection for current IP address only works on Linux!
  """
  # external function
  # returns:
  #  success: output of unzip command
  #  failure: None
  def unpackFile(self, filename):
    devroot = self.getDeviceRoot()
    if (devroot == None):
      return None

    dir = ''
    parts = filename.split('/')
    if (len(parts) > 1):
      if self.fileExists(filename):
        dir = '/'.join(parts[:-1])
    elif self.fileExists('/' + filename):
      dir = '/' + filename
    elif self.fileExists(devroot + '/' + filename):
      dir = devroot + '/' + filename
    else:
      return None

    try:
      data = self.runCmds(['cd ' + dir, 'unzp ' + filename])
    except AgentError:
      return None

    return data

  def getCallbackIpAndPort(self, aIp, aPort):
    ip = aIp
    nettools = NetworkTools()
    if (ip == None):
      ip = nettools.getLanIp()
    if (aPort != None):
      port = nettools.findOpenPort(ip, aPort)
    else:
      port = nettools.findOpenPort(ip, 30000)
    return ip, port

  """
    Returns a properly formatted env string for the agent.
    Input - env, which is either None, '', or a dict
    Output - a quoted string of the form: '"envvar1=val1,envvar2=val2..."'
    If env is None or '' return '' (empty quoted string)
  """
  def formatEnvString(self, env):
    if (env == None or env == ''):
      return ''

    retVal = '"%s"' % ','.join(map(lambda x: '%s=%s' % (x[0], x[1]), env.iteritems()))
    if (retVal == '""'):
      return ''

    return retVal

  """
    adjust the screen resolution on the device, REBOOT REQUIRED
    NOTE: this only works on a tegra ATM
    success: True
    failure: False

    supported resolutions: 640x480, 800x600, 1024x768, 1152x864, 1200x1024, 1440x900, 1680x1050, 1920x1080
  """
  def adjustResolution(self, width=1680, height=1050, type='hdmi'):
    if self.getInfo('os')['os'][0].split()[0] != 'harmony-eng':
      if (self.debug >= 2): print "WARNING: unable to adjust screen resolution on non Tegra device"
      return False

    results = self.getInfo('screen')
    parts = results['screen'][0].split(':')
    if (self.debug >= 3): print "INFO: we have a current resolution of %s, %s" % (parts[1].split()[0], parts[2].split()[0])

    #verify screen type is valid, and set it to the proper value (https://bugzilla.mozilla.org/show_bug.cgi?id=632895#c4)
    screentype = -1
    if (type == 'hdmi'):
      screentype = 5
    elif (type == 'vga' or type == 'crt'):
      screentype = 3
    else:
      return False

    #verify we have numbers
    if not (isinstance(width, int) and isinstance(height, int)):
      return False

    if (width < 100 or width > 9999):
      return False

    if (height < 100 or height > 9999):
      return False

    if (self.debug >= 3): print "INFO: adjusting screen resolution to %s, %s and rebooting" % (width, height)
    try:
      self.runCmds(["exec setprop persist.tegra.dpy%s.mode.width %s" % (screentype, width)])
      self.runCmds(["exec setprop persist.tegra.dpy%s.mode.height %s" % (screentype, height)])
    except AgentError:
      return False

    return True

  # external function
  # returns:
  #  success: True
  #  failure: False
  def chmodDir(self, remoteDir):
    try:
      self.runCmds(["chmod "+remoteDir])
    except AgentError:
      return False
    return True

gCallbackData = ''

class myServer(SocketServer.TCPServer):
  allow_reuse_address = True

class callbackServer():
  def __init__(self, ip, port, debuglevel):
    global gCallbackData
    if (debuglevel >= 1): print "DEBUG: gCallbackData is: %s on port: %s" % (gCallbackData, port)
    gCallbackData = ''
    self.ip = ip
    self.port = port
    self.connected = False
    self.debug = debuglevel
    if (self.debug >= 3): print "Creating server with " + str(ip) + ":" + str(port)
    self.server = myServer((ip, port), self.myhandler)
    self.server_thread = Thread(target=self.server.serve_forever) 
    self.server_thread.setDaemon(True)
    self.server_thread.start()

  def disconnect(self, step = 60, timeout = 600):
    t = 0
    if (self.debug >= 3): print "Calling disconnect on callback server"
    while t < timeout:
      if (gCallbackData):
        # Got the data back
        if (self.debug >= 3): print "Got data back from agent: " + str(gCallbackData)
        break
      else:
        if (self.debug >= 0): print '.',
      time.sleep(step)
      t += step

    try:
      if (self.debug >= 3): print "Shutting down server now"
      self.server.shutdown()
    except:
      if (self.debug >= 1): print "Unable to shutdown callback server - check for a connection on port: " + str(self.port)

    #sleep 1 additional step to ensure not only we are online, but all our services are online
    time.sleep(step)
    return gCallbackData

  class myhandler(SocketServer.BaseRequestHandler):
    def handle(self):
      global gCallbackData
      gCallbackData = self.request.recv(1024)
      #print "Callback Handler got data: " + str(gCallbackData)
      self.request.send("OK")
  
