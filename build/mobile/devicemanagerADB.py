# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import subprocess
from devicemanager import DeviceManager, DMError, _pop_last_line
import re
import os
import sys
import tempfile

class DeviceManagerADB(DeviceManager):

  def __init__(self, host=None, port=20701, retrylimit=5, packageName='fennec',
               adbPath='adb', deviceSerial=None, deviceRoot=None):
    self.host = host
    self.port = port
    self.retrylimit = retrylimit
    self.retries = 0
    self._sock = None
    self.useRunAs = False
    self.haveRoot = False
    self.useDDCopy = False
    self.useZip = False
    self.packageName = None
    self.tempDir = None
    self.deviceRoot = deviceRoot

    # the path to adb, or 'adb' to assume that it's on the PATH
    self.adbPath = adbPath

    # The serial number of the device to use with adb, used in cases
    # where multiple devices are being managed by the same adb instance.
    self.deviceSerial = deviceSerial

    if packageName == 'fennec':
      if os.getenv('USER'):
        self.packageName = 'org.mozilla.fennec_' + os.getenv('USER')
      else:
        self.packageName = 'org.mozilla.fennec_'
    elif packageName:
      self.packageName = packageName

    # verify that we can run the adb command. can't continue otherwise
    self.verifyADB()

    # try to connect to the device over tcp/ip if we have a hostname
    if self.host:
      self.connectRemoteADB()

    # verify that we can connect to the device. can't continue
    self.verifyDevice()

    # set up device root
    self.setupDeviceRoot()

    # Can we use run-as? (currently not required)
    try:
      self.verifyRunAs()
    except DMError:
      pass

    # Can we run things as root? (currently not required)
    useRunAsTmp = self.useRunAs
    self.useRunAs = False
    try:
      self.verifyRoot()
    except DMError, e:
      try:
        self.checkCmd(["root"])
        # The root command does not fail even if ADB cannot get
        # root rights (e.g. due to production builds), so we have
        # to check again ourselves that we have root now.
        self.verifyRoot()
      except DMError:
        if useRunAsTmp:
          print "restarting as root failed, but run-as available"
        else:
          print "restarting as root failed"
    self.useRunAs = useRunAsTmp

    # can we use zip to speed up some file operations? (currently not
    # required)
    try:
      self.verifyZip()
    except DMError:
      pass

  def __del__(self):
    if self.host:
      self.disconnectRemoteADB()

  # external function: executes shell command on device
  # returns:
  # success: <return code>
  # failure: None
  def shell(self, cmd, outputfile, env=None, cwd=None):
    # FIXME: this function buffers all output of the command into memory,
    # always. :(

    # Getting the return code is more complex than you'd think because adb
    # doesn't actually return the return code from a process, so we have to
    # capture the output to get it
    cmdline = "%s; echo $?" % self._escapedCommandLine(cmd)

    # prepend cwd and env to command if necessary
    if cwd:
      cmdline = "cd %s; %s" % (cwd, cmdline)
    if env:
      envstr = '; '.join(map(lambda x: 'export %s=%s' % (x[0], x[1]), env.iteritems()))
      cmdline = envstr + "; " + cmdline

    # all output should be in stdout
    args=[self.adbPath]
    if self.deviceSerial:
        args.extend(['-s', self.deviceSerial])
    args.extend(["shell", cmdline])
    proc = subprocess.Popen(args,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    (stdout, stderr) = proc.communicate()
    outputfile.write(stdout.rstrip('\n'))

    lastline = _pop_last_line(outputfile)
    if lastline:
      m = re.search('([0-9]+)', lastline)
      if m:
        return_code = m.group(1)
        outputfile.seek(-2, 2)
        outputfile.truncate() # truncate off the return code
        return int(return_code)

    return None

  def connectRemoteADB(self):
    self.checkCmd(["connect", self.host + ":" + str(self.port)])

  def disconnectRemoteADB(self):
    self.checkCmd(["disconnect", self.host + ":" + str(self.port)])

  # external function
  # returns:
  #  success: True
  #  failure: False
  def pushFile(self, localname, destname):
    try:
      if (os.name == "nt"):
        destname = destname.replace('\\', '/')
      if (self.useRunAs):
        remoteTmpFile = self.getTempDir() + "/" + os.path.basename(localname)
        self.checkCmd(["push", os.path.realpath(localname), remoteTmpFile])
        if self.useDDCopy:
          self.checkCmdAs(["shell", "dd", "if=" + remoteTmpFile, "of=" + destname])
        else:
          self.checkCmdAs(["shell", "cp", remoteTmpFile, destname])
        self.checkCmd(["shell", "rm", remoteTmpFile])
      else:
        self.checkCmd(["push", os.path.realpath(localname), destname])
      if (self.isDir(destname)):
        destname = destname + "/" + os.path.basename(localname)
      return True
    except:
      return False

  # external function
  # returns:
  #  success: directory name
  #  failure: None
  def mkDir(self, name):
    try:
      result = self.runCmdAs(["shell", "mkdir", name]).stdout.read()
      if 'read-only file system' in result.lower():
        return None
      if 'file exists' in result.lower():
        return name
      return name
    except:
      return None

  # push localDir from host to remoteDir on the device
  # external function
  # returns:
  #  success: remoteDir
  #  failure: None
  def pushDir(self, localDir, remoteDir):
    # adb "push" accepts a directory as an argument, but if the directory
    # contains symbolic links, the links are pushed, rather than the linked
    # files; we either zip/unzip or push file-by-file to get around this 
    # limitation
    try:
      if (not self.dirExists(remoteDir)):
        self.mkDirs(remoteDir+"/x")
      if (self.useZip):
        try:
          localZip = tempfile.mktemp()+".zip"
          remoteZip = remoteDir + "/adbdmtmp.zip"
          subprocess.check_output(["zip", "-r", localZip, '.'], cwd=localDir)
          self.pushFile(localZip, remoteZip)
          os.remove(localZip)
          data = self.runCmdAs(["shell", "unzip", "-o", remoteZip, "-d", remoteDir]).stdout.read()
          self.checkCmdAs(["shell", "rm", remoteZip])
          if (re.search("unzip: exiting", data) or re.search("Operation not permitted", data)):
            raise Exception("unzip failed, or permissions error")
        except:
          print "zip/unzip failure: falling back to normal push"
          self.useZip = False
          self.pushDir(localDir, remoteDir)
      else:
        for root, dirs, files in os.walk(localDir, followlinks=True):
          relRoot = os.path.relpath(root, localDir)
          for file in files:
            localFile = os.path.join(root, file)
            remoteFile = remoteDir + "/"
            if (relRoot!="."):
              remoteFile = remoteFile + relRoot + "/"
            remoteFile = remoteFile + file
            self.pushFile(localFile, remoteFile)
          for dir in dirs:
            targetDir = remoteDir + "/"
            if (relRoot!="."):
              targetDir = targetDir + relRoot + "/"
            targetDir = targetDir + dir
            if (not self.dirExists(targetDir)):
              self.mkDir(targetDir)
      return remoteDir
    except:
      print "pushing " + localDir + " to " + remoteDir + " failed"
      return None

  # external function
  # returns:
  #  success: True
  #  failure: False
  def dirExists(self, dirname):
    return self.isDir(dirname)

  # Because we always have / style paths we make this a lot easier with some
  # assumptions
  # external function
  # returns:
  #  success: True
  #  failure: False
  def fileExists(self, filepath):
    p = self.runCmd(["shell", "ls", "-a", filepath])
    data = p.stdout.readlines()
    if (len(data) == 1):
      if (data[0].rstrip() == filepath):
        return True
    return False

  def removeFile(self, filename):
    return self.runCmd(["shell", "rm", filename]).stdout.read()

  # does a recursive delete of directory on the device: rm -Rf remoteDir
  # external function
  # returns:
  #  success: output of telnet, i.e. "removing file: /mnt/sdcard/tests/test.txt"
  #  failure: None
  def removeSingleDir(self, remoteDir):
    return self.runCmd(["shell", "rmdir", remoteDir]).stdout.read()

  # does a recursive delete of directory on the device: rm -Rf remoteDir
  # external function
  # returns:
  #  success: output of telnet, i.e. "removing file: /mnt/sdcard/tests/test.txt"
  #  failure: None
  def removeDir(self, remoteDir):
      out = ""
      if (self.isDir(remoteDir)):
          files = self.listFiles(remoteDir.strip())
          for f in files:
              if (self.isDir(remoteDir.strip() + "/" + f.strip())):
                  out += self.removeDir(remoteDir.strip() + "/" + f.strip())
              else:
                  out += self.removeFile(remoteDir.strip() + "/" + f.strip())
          out += self.removeSingleDir(remoteDir.strip())
      else:
          out += self.removeFile(remoteDir.strip())
      return out

  def isDir(self, remotePath):
      p = self.runCmd(["shell", "ls", "-a", remotePath])
      data = p.stdout.readlines()
      if (len(data) == 0):
          return True
      if (len(data) == 1):
          if (data[0].rstrip() == remotePath):
              return False
          if (data[0].find("No such file or directory") != -1):
              return False
          if (data[0].find("Not a directory") != -1):
              return False
      return True

  def listFiles(self, rootdir):
      p = self.runCmd(["shell", "ls", "-a", rootdir])
      data = p.stdout.readlines()
      data[:] = [item.rstrip('\r\n') for item in data]
      if (len(data) == 1):
          if (data[0] == rootdir):
              return []
          if (data[0].find("No such file or directory") != -1):
              return []
          if (data[0].find("Not a directory") != -1):
              return []
          if (data[0].find("Permission denied") != -1):
              return []
          if (data[0].find("opendir failed") != -1):
              return []
      return data

  # external function
  # returns:
  #  success: array of process tuples
  #  failure: []
  def getProcessList(self):
    p = self.runCmd(["shell", "ps"])
      # first line is the headers
    p.stdout.readline()
    proc = p.stdout.readline()
    ret = []
    while (proc):
      els = proc.split()
      ret.append(list([els[1], els[len(els) - 1], els[0]]))
      proc =  p.stdout.readline()
    return ret

  # external function
  # DEPRECATED: Use shell() or launchApplication() for new code
  # returns:
  #  success: pid
  #  failure: None
  def fireProcess(self, appname, failIfRunning=False):
    #strip out env vars
    parts = appname.split('"');
    if (len(parts) > 2):
      parts = parts[2:]
    return self.launchProcess(parts, failIfRunning)

  # external function
  # DEPRECATED: Use shell() or launchApplication() for new code
  # returns:
  #  success: output filename
  #  failure: None
  def launchProcess(self, cmd, outputFile = "process.txt", cwd = '', env = '', failIfRunning=False):
    if cmd[0] == "am":
      self.checkCmd(["shell"] + cmd)
      return outputFile

    acmd = ["shell", "am", "start", "-W"]
    cmd = ' '.join(cmd).strip()
    i = cmd.find(" ")
    # SUT identifies the URL by looking for :\\ -- another strategy to consider
    re_url = re.compile('^[http|file|chrome|about].*')
    last = cmd.rfind(" ")
    uri = ""
    args = ""
    if re_url.match(cmd[last:].strip()):
      args = cmd[i:last].strip()
      uri = cmd[last:].strip()
    else:
      args = cmd[i:].strip()
    acmd.append("-n")
    acmd.append(cmd[0:i] + "/.App")
    if args != "":
      acmd.append("--es")
      acmd.append("args")
      acmd.append(args)
    if env != '' and env != None:
      envCnt = 0
      # env is expected to be a dict of environment variables
      for envkey, envval in env.iteritems():
        acmd.append("--es")
        acmd.append("env" + str(envCnt))
        acmd.append(envkey + "=" + envval);
        envCnt += 1
    if uri != "":
      acmd.append("-d")
      acmd.append(''.join(['\'',uri, '\'']));
    print acmd
    self.checkCmd(acmd)
    return outputFile

  # external function
  # returns:
  #  success: True
  #  failure: False
  def killProcess(self, appname, forceKill=False):
    procs = self.getProcessList()
    didKillProcess = False
    for (pid, name, user) in procs:
      if name == appname:
         args = ["shell", "kill"]
         if forceKill:
           args.append("-9")
         args.append(pid)
         p = self.runCmdAs(args)
         didKillProcess = True

    return didKillProcess

  # external function
  # returns:
  #  success: filecontents
  #  failure: None
  def catFile(self, remoteFile):
    #p = self.runCmd(["shell", "cat", remoteFile])
    #return p.stdout.read()
    return self.getFile(remoteFile)

  # external function
  # returns:
  #  success: output of pullfile, string
  #  failure: None
  def pullFile(self, remoteFile):
    #return self.catFile(remoteFile)
    return self.getFile(remoteFile)

  # copy file from device (remoteFile) to host (localFile)
  # external function
  # returns:
  #  success: output of pullfile, string
  #  failure: None
  def getFile(self, remoteFile, localFile = 'tmpfile_dm_adb'):
    # TODO: add debug flags and allow for printing stdout
    # self.runCmd(["pull", remoteFile, localFile])
    try:

      # First attempt to pull file regularly
      outerr = self.runCmd(["pull",  remoteFile, localFile]).communicate()

      # Now check stderr for errors
      if outerr[1]:
        errl = outerr[1].splitlines()
        if (len(errl) == 1):
          if (((errl[0].find("Permission denied") != -1)
            or (errl[0].find("does not exist") != -1))
            and self.useRunAs):
            # If we lack permissions to read but have run-as, then we should try
            # to copy the file to a world-readable location first before attempting
            # to pull it again.
            remoteTmpFile = self.getTempDir() + "/" + os.path.basename(remoteFile)
            self.checkCmdAs(["shell", "dd", "if=" + remoteFile, "of=" + remoteTmpFile])
            self.checkCmdAs(["shell", "chmod", "777", remoteTmpFile])
            self.runCmd(["pull",  remoteTmpFile, localFile]).stdout.read()
            # Clean up temporary file
            self.checkCmdAs(["shell", "rm", remoteTmpFile])

      f = open(localFile)
      ret = f.read()
      f.close()
      return ret
    except:
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
    ret = []
    p = self.runCmd(["pull", remoteDir, localDir])
    p.stdout.readline()
    line = p.stdout.readline()
    while (line):
      els = line.split()
      f = els[len(els) - 1]
      i = f.find(localDir)
      if (i != -1):
        if (localDir[len(localDir) - 1] != '/'):
          i = i + 1
        f = f[i + len(localDir):]
      i = f.find("/")
      if (i > 0):
        f = f[0:i]
      ret.append(f)
      line =  p.stdout.readline()
    #the last line is a summary
    if (len(ret) > 0):
      ret.pop()
    return ret



  # true/false check if the two files have the same md5 sum
  # external function
  # returns:
  #  success: True
  #  failure: False
  def validateFile(self, remoteFile, localFile):
    return self.getRemoteHash(remoteFile) == self.getLocalHash(localFile)

  # return the md5 sum of a remote file
  # internal function
  # returns:
  #  success: MD5 hash for given filename
  #  failure: None
  def getRemoteHash(self, filename):
    data = p = self.runCmd(["shell", "ls", "-l", filename]).stdout.read()
    return data.split()[3]

  def getLocalHash(self, filename):
    data = p = subprocess.Popen(["ls", "-l", filename], stdout=subprocess.PIPE).stdout.read()
    return data.split()[4]

  # Internal method to setup the device root and cache its value
  def setupDeviceRoot(self):
    # if self.deviceRoot is already set, create it if necessary, and use it
    if self.deviceRoot:
      if not self.dirExists(self.deviceRoot):
        if not self.mkDir(self.deviceRoot):
          raise DMError("Unable to create device root %s" % self.deviceRoot)
      return

    # /mnt/sdcard/tests is preferred to /data/local/tests, but this can be
    # over-ridden by creating /data/local/tests
    testRoot = "/data/local/tests"
    if (self.dirExists(testRoot)):
      self.deviceRoot = testRoot
      return

    for (basePath, subPath) in [('/mnt/sdcard', 'tests'),
                                ('/data/local', 'tests')]:
      if self.dirExists(basePath):
        testRoot = os.path.join(basePath, subPath)
        if self.mkDir(testRoot):
          self.deviceRoot = testRoot
          return

    raise DMError("Unable to set up device root as /mnt/sdcard/tests "
                  "or /data/local/tests")

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
    return self.deviceRoot

  # Gets the temporary directory we are using on this device
  # base on our device root, ensuring also that it exists.
  #
  # internal function
  # returns:
  #  success: path for temporary directory
  #  failure: None
  def getTempDir(self):
    # Cache result to speed up operations depending
    # on the temporary directory.
    if self.tempDir == None:
      self.tempDir = self.getDeviceRoot() + "/tmp"
      if (not self.dirExists(self.tempDir)):
        return self.mkDir(self.tempDir)

    return self.tempDir

  # Either we will have /tests/fennec or /tests/firefox but we will never have
  # both.  Return the one that exists
  # TODO: ensure we can support org.mozilla.firefox
  # external function
  # returns:
  #  success: path for app root
  #  failure: None
  def getAppRoot(self, packageName):
    devroot = self.getDeviceRoot()
    if (devroot == None):
      return None

    if (packageName and self.dirExists('/data/data/' + packageName)):
      self.packageName = packageName
      return '/data/data/' + packageName
    elif (self.packageName and self.dirExists('/data/data/' + self.packageName)):
      return '/data/data/' + self.packageName

    # Failure (either not installed or not a recognized platform)
    print "devicemanagerADB: getAppRoot failed"
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


  # external function
  # returns:
  #  success: status from test agent
  #  failure: None
  def reboot(self, wait = False):
    ret = self.runCmd(["reboot"]).stdout.read()
    if (not wait):
      return "Success"
    countdown = 40
    while (countdown > 0):
      countdown
      try:
        self.checkCmd(["wait-for-device", "shell", "ls", "/sbin"])
        return ret
      except:
        try:
          self.checkCmd(["root"])
        except:
          time.sleep(1)
          print "couldn't get root"
    return "Success"

  # external function
  # returns:
  #  success: text status from command or callback server
  #  failure: None
  def updateApp(self, appBundlePath, processName=None, destPath=None, ipAddr=None, port=30000):
    return self.runCmd(["install", "-r", appBundlePath]).stdout.read()

  # external function
  # returns:
  #  success: time in ms
  #  failure: None
  def getCurrentTime(self):
    timestr = self.runCmd(["shell", "date", "+%s"]).stdout.read().strip()
    if (not timestr or not timestr.isdigit()):
        return None
    return str(int(timestr)*1000)

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
  def getInfo(self, directive="all"):
    ret = {}
    if (directive == "id" or directive == "all"):
      ret["id"] = self.runCmd(["get-serialno"]).stdout.read()
    if (directive == "os" or directive == "all"):
      ret["os"] = self.runCmd(["shell", "getprop", "ro.build.display.id"]).stdout.read()
    if (directive == "uptime" or directive == "all"):
      utime = self.runCmd(["shell", "uptime"]).stdout.read()
      if (not utime):
        raise DMError("error getting uptime")
      utime = utime[9:]
      hours = utime[0:utime.find(":")]
      utime = utime[utime[1:].find(":") + 2:]
      minutes = utime[0:utime.find(":")]
      utime = utime[utime[1:].find(":") +  2:]
      seconds = utime[0:utime.find(",")]
      ret["uptime"] = ["0 days " + hours + " hours " + minutes + " minutes " + seconds + " seconds"]
    if (directive == "process" or directive == "all"):
      ret["process"] = self.runCmd(["shell", "ps"]).stdout.read()
    if (directive == "systime" or directive == "all"):
      ret["systime"] = self.runCmd(["shell", "date"]).stdout.read()
    print ret
    return ret

  def runCmd(self, args):
    # If we are not root but have run-as, and we're trying to execute 
    # a shell command then using run-as is the best we can do
    finalArgs = [self.adbPath]
    if self.deviceSerial:
      finalArgs.extend(['-s', self.deviceSerial])
    if (not self.haveRoot and self.useRunAs and args[0] == "shell" and args[1] != "run-as"):
      args.insert(1, "run-as")
      args.insert(2, self.packageName)
    finalArgs.extend(args)
    return subprocess.Popen(finalArgs, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

  def runCmdAs(self, args):
    if self.useRunAs:
      args.insert(1, "run-as")
      args.insert(2, self.packageName)
    return self.runCmd(args)

  def checkCmd(self, args):
    # If we are not root but have run-as, and we're trying to execute 
    # a shell command then using run-as is the best we can do
    finalArgs = [self.adbPath]
    if self.deviceSerial:
      finalArgs.extend(['-s', self.deviceSerial])
    if (not self.haveRoot and self.useRunAs and args[0] == "shell" and args[1] != "run-as"):
      args.insert(1, "run-as")
      args.insert(2, self.packageName)
    finalArgs.extend(args)
    return subprocess.check_call(finalArgs)

  def checkCmdAs(self, args):
    if (self.useRunAs):
      args.insert(1, "run-as")
      args.insert(2, self.packageName)
    return self.checkCmd(args)

  # external function
  # returns:
  #  success: True
  #  failure: False
  def chmodDir(self, remoteDir):
    if (self.isDir(remoteDir)):
      files = self.listFiles(remoteDir.strip())
      for f in files:
        remoteEntry = remoteDir.strip() + "/" + f.strip()
        if (self.isDir(remoteEntry)):
          self.chmodDir(remoteEntry)
        else:
          self.checkCmdAs(["shell", "chmod", "777", remoteEntry])
          print "chmod " + remoteEntry
      self.checkCmdAs(["shell", "chmod", "777", remoteDir])
      print "chmod " + remoteDir
    else:
      self.checkCmdAs(["shell", "chmod", "777", remoteDir.strip()])
      print "chmod " + remoteDir.strip()
    return True

  def verifyADB(self):
    # Check to see if adb itself can be executed.
    if self.adbPath != 'adb':
      if not os.access(self.adbPath, os.X_OK):
        raise DMError("invalid adb path, or adb not executable: %s", self.adbPath)

    try:
      self.checkCmd(["version"])
    except os.error, err:
      raise DMError("unable to execute ADB (%s): ensure Android SDK is installed and adb is in your $PATH" % err)
    except subprocess.CalledProcessError:
      raise DMError("unable to execute ADB: ensure Android SDK is installed and adb is in your $PATH")

  def verifyDevice(self):
    # If there is a device serial number, see if adb is connected to it
    if self.deviceSerial:
      deviceStatus = None
      proc = subprocess.Popen([self.adbPath, "devices"],
                              stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT)
      for line in proc.stdout:
        m = re.match('(.+)?\s+(.+)$', line)
        if m:
          if self.deviceSerial == m.group(1):
            deviceStatus = m.group(2)
      if deviceStatus == None:
        raise DMError("device not found: %s" % self.deviceSerial)
      elif deviceStatus != "device":
        raise DMError("bad status for device %s: %s" % (self.deviceSerial,
                                                        deviceStatus))

    # Check to see if we can connect to device and run a simple command
    try:
      self.checkCmd(["shell", "echo"])
    except subprocess.CalledProcessError:
      raise DMError("unable to connect to device: is it plugged in?")

  def verifyRoot(self):
    # a test to see if we have root privs
    files = self.listFiles("/data/data")
    if (len(files) == 0):
      print "NOT running as root"
      raise DMError("not running as root")

    self.haveRoot = True

  def isCpAvailable(self):
    # Some Android systems may not have a cp command installed,
    # or it may not be executable by the user.
    data = self.runCmd(["shell", "cp"]).stdout.read()
    if (re.search('Usage', data)):
      return True
    else:
      data = self.runCmd(["shell", "dd", "-"]).stdout.read()
      if (re.search('unknown operand', data)):
        print "'cp' not found, but 'dd' was found as a replacement"
        self.useDDCopy = True
        return True
      print "unable to execute 'cp' on device; consider installing busybox from Android Market"
      return False

  def verifyRunAs(self):
    # If a valid package name is available, and certain other
    # conditions are met, devicemanagerADB can execute file operations
    # via the "run-as" command, so that pushed files and directories 
    # are created by the uid associated with the package, more closely
    # echoing conditions encountered by Fennec at run time.
    # Check to see if run-as can be used here, by verifying a 
    # file copy via run-as.
    self.useRunAs = False
    devroot = self.getDeviceRoot()
    if (self.packageName and self.isCpAvailable() and devroot):
      tmpDir = self.getTempDir()

      # The problem here is that run-as doesn't cause a non-zero exit code
      # when failing because of a non-existent or non-debuggable package :(
      runAsOut = self.runCmd(["shell", "run-as", self.packageName, "mkdir", devroot + "/sanity"]).communicate()[0]
      if runAsOut.startswith("run-as:") and ("not debuggable" in runAsOut or
                                             "is unknown" in runAsOut):
        raise DMError("run-as failed sanity check")

      tmpfile = tempfile.NamedTemporaryFile()
      self.checkCmd(["push", tmpfile.name, tmpDir + "/tmpfile"])
      if self.useDDCopy:
        self.checkCmd(["shell", "run-as", self.packageName, "dd", "if=" + tmpDir + "/tmpfile", "of=" + devroot + "/sanity/tmpfile"])
      else:
        self.checkCmd(["shell", "run-as", self.packageName, "cp", tmpDir + "/tmpfile", devroot + "/sanity"])
      if (self.fileExists(devroot + "/sanity/tmpfile")):
        print "will execute commands via run-as " + self.packageName
        self.useRunAs = True
      self.checkCmd(["shell", "rm", devroot + "/tmp/tmpfile"])
      self.checkCmd(["shell", "run-as", self.packageName, "rm", "-r", devroot + "/sanity"])

  def isUnzipAvailable(self):
    data = self.runCmdAs(["shell", "unzip"]).stdout.read()
    if (re.search('Usage', data)):
      return True
    else:
      return False

  def isLocalZipAvailable(self):
    try:
      subprocess.check_call(["zip", "-?"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except:
      return False
    return True

  def verifyZip(self):
    # If "zip" can be run locally, and "unzip" can be run remotely, then pushDir
    # can use these to push just one file per directory -- a significant
    # optimization for large directories.
    self.useZip = False
    if (self.isUnzipAvailable() and self.isLocalZipAvailable()):
      print "will use zip to push directories"
      self.useZip = True
    else:
      raise DMError("zip not available")
