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
# The Original Code is the Python XPCOM language bindings.
#
# The Initial Developer of the Original Code is
# ActiveState Tool Corp.
# Portions created by the Initial Developer are Copyright (C) 2000, 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Mark Hammond <MarkH@ActiveState.com> (original author)
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

"""Implementation of Python file objects for Mozilla/xpcom.

Introduction:
  This module defines various class that are implemented using 
  Mozilla streams.  This allows you to open Mozilla URI's, and
  treat them as Python file object.
  
Example:
>>> file = URIFile("chrome://whatever")
>>> data = file.read(5) # Pass no arg to read everything.

Known Limitations:
 * Not all URL schemes will work from "python.exe" - most notably
   "chrome://" and "http://" URLs - this is because a simple initialization of
   xpcom by Python does not load up the full set of Mozilla URL handlers.
   If you can work out how to correctly initialize the chrome registry and
   setup a message queue.

Known Bugs:
  * Only read ("r") mode is supported.  Although write ("w") mode doesnt make
    sense for HTTP type URLs, it potentially does for file:// etc type ones.
  * No concept of text mode vs binary mode.  It appears Mozilla takes care of
    this internally (ie, all "text/???" mime types are text, rest are binary)

"""

from xpcom import components, Exception, _xpcom
import os
import threading # for locks.

NS_RDONLY        = 0x01
NS_WRONLY        = 0x02
NS_RDWR          = 0x04
NS_CREATE_FILE   = 0x08
NS_APPEND        = 0x10
NS_TRUNCATE      = 0x20
NS_SYNC          = 0x40
NS_EXCL          = 0x80

# A helper function that may come in useful
def LocalFileToURL(localFileName):
    "Convert a filename to an XPCOM nsIFileURL object."
    # Create an nsILocalFile
    localFile = components.classes["@mozilla.org/file/local;1"] \
          .createInstance(components.interfaces.nsILocalFile)
    localFile.initWithPath(localFileName)

    # Use the IO Service to create the interface, then QI for a FileURL
    io_service = components.classes["@mozilla.org/network/io-service;1"] \
                    .getService(components.interfaces.nsIIOService)
    url = io_service.newFileURI(localFile).queryInterface(components.interfaces.nsIFileURL)
    # Setting the "file" attribute causes initialization...
    url.file = localFile
    return url

# A base class for file objects.
class _File:
    def __init__(self, name_thingy = None, mode="r"):
        self.lockob = threading.Lock()
        self.inputStream = self.outputStream = None
        if name_thingy is not None:
                self.init(name_thingy, mode)

    def __del__(self):
        self.close()

    # The Moz file streams are not thread safe.
    def _lock(self):
        self.lockob.acquire()
    def _release(self):
        self.lockob.release()
    def read(self, n = -1):
        assert self.inputStream is not None, "Not setup for read!"
        self._lock()
        try:
            return str(self.inputStream.read(n))
        finally:
            self._release()

    def readlines(self):
        # Not part of the xpcom interface, but handy for direct Python users.
        # Not 100% faithful, but near enough for now!
        lines = self.read().split("\n")
        if len(lines) and len(lines[-1]) == 0:
            lines = lines[:-1]
        return [s+"\n" for s in lines ]

    def write(self, data):
        assert self.outputStream is not None, "Not setup for write!"
        self._lock()
        try:
            self.outputStream.write(data, len(data))
        finally:
            self._release()

    def close(self):
        self._lock()
        try:
            if self.inputStream is not None:
                self.inputStream.close()
                self.inputStream = None
            if self.outputStream is not None:
                self.outputStream.close()
                self.outputStream = None
            self.channel = None
        finally:
            self._release()

    def flush(self):
        self._lock()
        try:
            if self.outputStream is not None: self.outputStream.flush()
        finally:
            self._release()

# A synchronous "file object" used to open a URI.
class URIFile(_File):
    def init(self, url, mode="r"):
        self.close()
        if mode != "r":
            raise ValueError, "only 'r' mode supported'"
        io_service = components.classes["@mozilla.org/network/io-service;1"] \
                        .getService(components.interfaces.nsIIOService)
        if hasattr(url, "queryInterface"):
            url_ob = url
        else:
            url_ob = io_service.newURI(url, None, None)
        # Mozilla asserts and starts saying "NULL POINTER" if this is wrong!
        if not url_ob.scheme:
            raise ValueError, ("The URI '%s' is invalid (no scheme)" 
                                  % (url_ob.spec,))
        self.channel = io_service.newChannelFromURI(url_ob)
        self.inputStream = self.channel.open()

# A "file object" implemented using Netscape's native file support.
# Based on io.js - http://lxr.mozilla.org/seamonkey/source/xpcom/tests/utils/io.js
# You open this file using a local file name (as a string) so it really is pointless - 
# you may as well be using a standard Python file object!
class LocalFile(_File):
    def __init__(self, *args):
        self.fileIO = None
        _File.__init__(self, *args)

    def init(self, name, mode = "r"):
        name = os.path.abspath(name) # Moz libraries under Linux fail with relative paths.
        self.close()
        file = components.classes['@mozilla.org/file/local;1'].createInstance("nsILocalFile")
        file.initWithPath(name)
        if mode in ["w","a"]:
            self.fileIO = components.classes["@mozilla.org/network/file-output-stream;1"].createInstance("nsIFileOutputStream")
            if mode== "w":
                if file.exists():
                    file.remove(0)
                moz_mode = NS_CREATE_FILE | NS_WRONLY
            elif mode=="a":
                moz_mode = NS_APPEND
            else:
                assert 0, "Can't happen!"
            self.fileIO.init(file, moz_mode, -1,0)
            self.outputStream = self.fileIO
        elif mode == "r":
            self.fileIO = components.classes["@mozilla.org/network/file-input-stream;1"].createInstance("nsIFileInputStream")
            self.fileIO.init(file, NS_RDONLY, -1,0)
            self.inputStream = components.classes["@mozilla.org/scriptableinputstream;1"].createInstance("nsIScriptableInputStream")
            self.inputStream.init(self.fileIO)
        else:
            raise ValueError, "Unknown mode"

    def close(self):
        if self.fileIO is not None:
            self.fileIO.close()
            self.fileIO = None
        _File.close(self)

    def read(self, n = -1):
        return _File.read(self, n)


##########################################################
##
## Test Code
##
##########################################################
def _DoTestRead(file, expected):
    # read in a couple of chunks, just to test that our various arg combinations work.
    got = file.read(3)
    got = got + file.read(300)
    got = got + file.read(0)
    got = got + file.read()
    if got != expected:
        raise RuntimeError, "Reading '%s' failed - got %d bytes, but expected %d bytes" % (file, len(got), len(expected))

def _DoTestBufferRead(file, expected):
    # read in a couple of chunks, just to test that our various arg combinations work.
    buffer = _xpcom.AllocateBuffer(50)
    got = ''
    while 1:
        # Note - we need to reach into the file object so we
        # can get at the native buffer supported function.
        num = file.inputStream.read(buffer)
        if num == 0:
            break
        got = got + str(buffer[:num])
    if got != expected:
        raise RuntimeError, "Reading '%s' failed - got %d bytes, but expected %d bytes" % (file, len(got), len(expected))

def _TestLocalFile():
    import tempfile, os
    fname = tempfile.mktemp()
    data = "Hello from Python"
    test_file = LocalFile(fname, "w")
    try:
        test_file.write(data)
        test_file.close()
        # Make sure Python can read it OK.
        f = open(fname, "r")
        if f.read() != data:
                print "Eeek - Python could not read the data back correctly!"
        f.close()
        # For the sake of the test, try a re-init.
        test_file.init(fname, "r")
        got = str(test_file.read())
        if got != data:
            print "Read the wrong data back - %r" % (got,)
        else:
            print "Read the correct data."
        test_file.close()
        # Try reading in chunks.
        test_file = LocalFile(fname, "r")
        got = test_file.read(10) + test_file.read()
        if got != data:
            print "Chunks the wrong data back - %r" % (got,)
        else:
            print "Chunks read the correct data."
        test_file.close()
        # Open the same file again for writing - this should delete the old one.
        if not os.path.isfile(fname):
            raise RuntimeError, "The file '%s' does not exist, but we are explicitly testing create semantics when it does" % (fname,)
        test_file = LocalFile(fname, "w")
        test_file.write(data)
        test_file.close()
        # Make sure Python can read it OK.
        f = open(fname, "r")
        if f.read() != data:
                print "Eeek - Python could not read the data back correctly after recreating an existing file!"
        f.close()

        # XXX - todo - test "a" mode!
    finally:
        try:
            os.unlink(fname)
        except OSError, details:
            print "Error removing temp test file:", details

def _TestAll():
    # A mini test suite.
    # Get a test file, and convert it to a file:// URI.
    # check what we read is the same as when
    # we read this file "normally"
    fname = components.__file__
    if fname[-1] in "cCoO": # fix .pyc/.pyo
            fname = fname[:-1]
    expected = open(fname, "rb").read()
    # convert the fname to a URI.
    url = LocalFileToURL(fname)
    # First try passing a URL as a string.
    _DoTestRead( URIFile( url.spec), expected)
    print "Open as string test worked."
    # Now with a URL object.
    _DoTestRead( URIFile( url ), expected)
    print "Open as URL test worked."

    _DoTestBufferRead( URIFile( url ), expected)
    print "File test using buffers worked."

    # For the sake of testing, do our pointless, demo object!
    _DoTestRead( LocalFile(fname), expected )
    print "Local file read test worked."

    # Now do the full test of our pointless, demo object!
    _TestLocalFile()

def _TestURI(url):
    test_file = URIFile(url)
    print "Opened file is", test_file
    got = test_file.read()
    print "Read %d bytes of data from %r" % (len(got), url)
    test_file.close()

if __name__=='__main__':
    import sys
    if len(sys.argv) < 2:
        print "No URL specified on command line - performing self-test"
        _TestAll()
    else:
            _TestURI(sys.argv[1])
