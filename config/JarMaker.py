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
# The Original Code is Mozilla build system.
#
# The Initial Developer of the Original Code is
# Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2008
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Axel Hecht <l10n@mozilla.com>
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

'''jarmaker.py provides a python class to package up chrome content by
processing jar.mn files.

See the documentation for jar.mn on MDC for further details on the format.
'''

import sys
import os
import os.path
import errno
import re
import logging
from time import localtime
from optparse import OptionParser
from MozZipFile import ZipFile
from cStringIO import StringIO
from datetime import datetime

from utils import pushback_iter, lockFile
from Preprocessor import Preprocessor
from buildlist import addEntriesToListFile

__all__ = ['JarMaker']

class ZipEntry:
  '''Helper class for jar output.

  This class defines a simple file-like object for a zipfile.ZipEntry
  so that we can consecutively write to it and then close it.
  This methods hooks into ZipFile.writestr on close().
  '''
  def __init__(self, name, zipfile):
    self._zipfile = zipfile
    self._name = name
    self._inner = StringIO()

  def write(self, content):
    'Append the given content to this zip entry'
    self._inner.write(content)
    return

  def close(self):
    'The close method writes the content back to the zip file.'
    self._zipfile.writestr(self._name, self._inner.getvalue())

def getModTime(aPath):
  if not os.path.isfile(aPath):
    return 0
  mtime = os.stat(aPath).st_mtime
  return localtime(mtime)


class JarMaker(object):
  '''JarMaker reads jar.mn files and process those into jar files or
  flat directories, along with chrome.manifest files.
  '''

  ignore = re.compile('\s*(\#.*)?$')
  jarline = re.compile('(?:(?P<jarfile>[\w\d.\-\_\\\/]+).jar\:)|(?:\s*(\#.*)?)\s*$')
  regline = re.compile('\%\s+(.*)$')
  entryre = '(?P<optPreprocess>\*)?(?P<optOverwrite>\+?)\s+'
  entryline = re.compile(entryre + '(?P<output>[\w\d.\-\_\\\/\+]+)\s*(\((?P<locale>\%?)(?P<source>[\w\d.\-\_\\\/]+)\))?\s*$')

  def __init__(self, outputFormat = 'flat', useJarfileManifest = True,
               useChromeManifest = False):
    self.outputFormat = outputFormat
    self.useJarfileManifest = useJarfileManifest
    self.useChromeManifest = useChromeManifest
    self.pp = Preprocessor()

  def getCommandLineParser(self):
    '''Get a optparse.OptionParser for jarmaker.

    This OptionParser has the options for jarmaker as well as
    the options for the inner PreProcessor.
    '''
    # HACK, we need to unescape the string variables we get,
    # the perl versions didn't grok strings right
    p = self.pp.getCommandLineParser(unescapeDefines = True)
    p.add_option('-f', type="choice", default="jar",
                 choices=('jar', 'flat', 'symlink'),
                 help="fileformat used for output", metavar="[jar, flat, symlink]")
    p.add_option('-v', action="store_true", dest="verbose",
                 help="verbose output")
    p.add_option('-q', action="store_false", dest="verbose",
                 help="verbose output")
    p.add_option('-e', action="store_true",
                 help="create chrome.manifest instead of jarfile.manifest")
    p.add_option('--both-manifests', action="store_true",
                 dest="bothManifests",
                 help="create chrome.manifest and jarfile.manifest")
    p.add_option('-s', type="string", action="append", default=[],
                 help="source directory")
    p.add_option('-t', type="string",
                 help="top source directory")
    p.add_option('-c', '--l10n-src', type="string", action="append",
                 help="localization directory")
    p.add_option('--l10n-base', type="string", action="append", default=[],
                 help="base directory to be used for localization (multiple)")
    p.add_option('-j', type="string",
                 help="jarfile directory")
    # backwards compat, not needed
    p.add_option('-a', action="store_false", default=True,
                 help="NOT SUPPORTED, turn auto-registration of chrome off (installed-chrome.txt)")
    p.add_option('-d', type="string",
                 help="UNUSED, chrome directory")
    p.add_option('-o', help="cross compile for auto-registration, ignored")
    p.add_option('-l', action="store_true",
                 help="ignored (used to switch off locks)")
    p.add_option('-x', action="store_true",
                 help="force Unix")
    p.add_option('-z', help="backwards compat, ignored")
    p.add_option('-p', help="backwards compat, ignored")
    return p

  def processIncludes(self, includes):
    '''Process given includes with the inner PreProcessor.

    Only use this for #defines, the includes shouldn't generate
    content.
    '''
    self.pp.out = StringIO()
    for inc in includes:
      self.pp.do_include(inc)
    includesvalue = self.pp.out.getvalue()
    if includesvalue:
      logging.info("WARNING: Includes produce non-empty output")
    self.pp.out = None
    pass

  def finalizeJar(self, jarPath, chromebasepath, register,
                  doZip=True):
    '''Helper method to write out the chrome registration entries to
    jarfile.manifest or chrome.manifest, or both.

    The actual file processing is done in updateManifest.
    '''
    # rewrite the manifest, if entries given
    if not register:
      return

    chromeManifest = os.path.join(os.path.dirname(jarPath),
                                  '..', 'chrome.manifest')

    if self.useJarfileManifest:
      self.updateManifest(jarPath + '.manifest', chromebasepath % '',
                          register)
      addEntriesToListFile(chromeManifest, ['manifest chrome/%s.manifest' % (os.path.basename(jarPath),)])
    if self.useChromeManifest:
      self.updateManifest(chromeManifest, chromebasepath % 'chrome/',
                          register)

  def updateManifest(self, manifestPath, chromebasepath, register):
    '''updateManifest replaces the % in the chrome registration entries
    with the given chrome base path, and updates the given manifest file.
    '''
    lock = lockFile(manifestPath + '.lck')
    try:
      myregister = dict.fromkeys(map(lambda s: s.replace('%', chromebasepath),
                                     register.iterkeys()))
      manifestExists = os.path.isfile(manifestPath)
      mode = (manifestExists and 'r+b') or 'wb'
      mf = open(manifestPath, mode)
      if manifestExists:
        # import previous content into hash, ignoring empty ones and comments
        imf = re.compile('(#.*)?$')
        for l in re.split('[\r\n]+', mf.read()):
          if imf.match(l):
            continue
          myregister[l] = None
        mf.seek(0)
      for k in myregister.iterkeys():
        mf.write(k + os.linesep)
      mf.close()
    finally:
      lock = None
  
  def makeJar(self, infile=None,
               jardir='',
               sourcedirs=[], topsourcedir='', localedirs=None):
    '''makeJar is the main entry point to JarMaker.

    It takes the input file, the output directory, the source dirs and the
    top source dir as argument, and optionally the l10n dirs.
    '''
    if isinstance(infile, basestring):
      logging.info("processing " + infile)
    pp = self.pp.clone()
    pp.out = StringIO()
    pp.do_include(infile)
    lines = pushback_iter(pp.out.getvalue().splitlines())
    try:
      while True:
        l = lines.next()
        m = self.jarline.match(l)
        if not m:
          raise RuntimeError(l)
        if m.group('jarfile') is None:
          # comment
          continue
        self.processJarSection(m.group('jarfile'), lines,
                               jardir, sourcedirs, topsourcedir,
                               localedirs)
    except StopIteration:
      # we read the file
      pass
    return

  def makeJars(self, infiles, l10nbases,
               jardir='',
               sourcedirs=[], topsourcedir='', localedirs=None):
    '''makeJars is the second main entry point to JarMaker.

    It takes an iterable sequence of input file names, the l10nbases,
    the output directory, the source dirs and the
    top source dir as argument, and optionally the l10n dirs.

    It iterates over all inputs, guesses srcdir and l10ndir from the
    path and topsourcedir and calls into makeJar.

    The l10ndirs are created by guessing the relativesrcdir, and resolving
    that against the l10nbases. l10nbases can either be path strings, or 
    callables. In the latter case, that will be called with the 
    relativesrcdir as argument, and is expected to return a path string.
    This logic is disabled if the jar.mn path is not inside the topsrcdir.
    '''
    topsourcedir = os.path.normpath(os.path.abspath(topsourcedir))
    def resolveL10nBase(relpath):
      def _resolve(base):
        if isinstance(base, basestring):
          return os.path.join(base, relpath)
        if callable(base):
          return base(relpath)
        return base
      return _resolve
    for infile in infiles:
      srcdir = os.path.normpath(os.path.abspath(os.path.dirname(infile)))
      l10ndir = srcdir
      if os.path.basename(srcdir) == 'locales':
        l10ndir = os.path.dirname(l10ndir)

      l10ndirs = None
      # srcdir may not be a child of topsourcedir, in which case
      # we assume that the caller passed in suitable sourcedirs,
      # and just skip passing in localedirs
      if srcdir.startswith(topsourcedir):
        rell10ndir = l10ndir[len(topsourcedir):].lstrip(os.sep)

        l10ndirs = map(resolveL10nBase(rell10ndir), l10nbases)
        if localedirs is not None:
          l10ndirs += [os.path.normpath(os.path.abspath(s))
                       for s in localedirs]
      srcdirs = [os.path.normpath(os.path.abspath(s))
                 for s in sourcedirs] + [srcdir]
      self.makeJar(infile=infile,
                   sourcedirs=srcdirs, topsourcedir=topsourcedir,
                   localedirs=l10ndirs,
                   jardir=jardir)


  def processJarSection(self, jarfile, lines,
                        jardir, sourcedirs, topsourcedir, localedirs):
    '''Internal method called by makeJar to actually process a section
    of a jar.mn file.

    jarfile is the basename of the jarfile or the directory name for 
    flat output, lines is a pushback_iterator of the lines of jar.mn,
    the remaining options are carried over from makeJar.
    '''

    # chromebasepath is used for chrome registration manifests
    # %s is getting replaced with chrome/ for chrome.manifest, and with
    # an empty string for jarfile.manifest
    chromebasepath = '%s' + jarfile
    if self.outputFormat == 'jar':
      chromebasepath = 'jar:' + chromebasepath + '.jar!'
    chromebasepath += '/'

    jarfile = os.path.join(jardir, jarfile)
    jf = None
    if self.outputFormat == 'jar':
      #jar
      jarfilepath = jarfile + '.jar'
      try:
        os.makedirs(os.path.dirname(jarfilepath))
      except OSError:
        pass
      jf = ZipFile(jarfilepath, 'a', lock = True)
      outHelper = self.OutputHelper_jar(jf)
    else:
      outHelper = getattr(self, 'OutputHelper_' + self.outputFormat)(jarfile)
    register = {}
    # This loop exits on either
    # - the end of the jar.mn file
    # - an line in the jar.mn file that's not part of a jar section
    # - on an exception raised, close the jf in that case in a finally
    try:
      while True:
        try:
          l = lines.next()
        except StopIteration:
          # we're done with this jar.mn, and this jar section
          self.finalizeJar(jarfile, chromebasepath, register)
          if jf is not None:
            jf.close()
          # reraise the StopIteration for makeJar
          raise
        if self.ignore.match(l):
          continue
        m = self.regline.match(l)
        if  m:
          rline = m.group(1)
          register[rline] = 1
          continue
        m = self.entryline.match(l)
        if not m:
          # neither an entry line nor chrome reg, this jar section is done
          self.finalizeJar(jarfile, chromebasepath, register)
          if jf is not None:
            jf.close()
          lines.pushback(l)
          return
        self._processEntryLine(m, sourcedirs, topsourcedir, localedirs,
                              outHelper, jf)
    finally:
      if jf is not None:
        jf.close()
    return

  def _processEntryLine(self, m, 
                        sourcedirs, topsourcedir, localedirs,
                        outHelper, jf):
      out = m.group('output')
      src = m.group('source') or os.path.basename(out)
      # pick the right sourcedir -- l10n, topsrc or src
      if m.group('locale'):
        src_base = localedirs
      elif src.startswith('/'):
        # path/in/jar/file_name.xul     (/path/in/sourcetree/file_name.xul)
        # refers to a path relative to topsourcedir, use that as base
        # and strip the leading '/'
        src_base = [topsourcedir]
        src = src[1:]
      else:
        # use srcdirs and the objdir (current working dir) for relative paths
        src_base = sourcedirs + ['.']
      # check if the source file exists
      realsrc = None
      for _srcdir in src_base:
        if os.path.isfile(os.path.join(_srcdir, src)):
          realsrc = os.path.join(_srcdir, src)
          break
      if realsrc is None:
        if jf is not None:
          jf.close()
        raise RuntimeError('File "%s" not found in %s' % (src, ', '.join(src_base)))
      if m.group('optPreprocess'):
        outf = outHelper.getOutput(out)
        inf = open(realsrc)
        pp = self.pp.clone()
        if src[-4:] == '.css':
          pp.setMarker('%')
        pp.out = outf
        pp.do_include(inf)
        outf.close()
        inf.close()
        return
      # copy or symlink if newer or overwrite
      if (m.group('optOverwrite')
          or (getModTime(realsrc) >
              outHelper.getDestModTime(m.group('output')))):
        if self.outputFormat == 'symlink' and hasattr(os, 'symlink'):
          outHelper.symlink(realsrc, out)
          return
        outf = outHelper.getOutput(out)
        # open in binary mode, this can be images etc
        inf = open(realsrc, 'rb')
        outf.write(inf.read())
        outf.close()
        inf.close()
    

  class OutputHelper_jar(object):
    '''Provide getDestModTime and getOutput for a given jarfile.
    '''
    def __init__(self, jarfile):
      self.jarfile = jarfile
    def getDestModTime(self, aPath):
      try :
        info = self.jarfile.getinfo(aPath)
        return info.date_time
      except:
        return 0
    def getOutput(self, name):
      return ZipEntry(name, self.jarfile)

  class OutputHelper_flat(object):
    '''Provide getDestModTime and getOutput for a given flat
    output directory. The helper method ensureDirFor is used by
    the symlink subclass.
    '''
    def __init__(self, basepath):
      self.basepath = basepath
    def getDestModTime(self, aPath):
      return getModTime(os.path.join(self.basepath, aPath))
    def getOutput(self, name):
      out = self.ensureDirFor(name)
      # remove previous link or file
      try:
        os.remove(out)
      except OSError, e:
        if e.errno != errno.ENOENT:
          raise
      return open(out, 'wb')
    def ensureDirFor(self, name):
      out = os.path.join(self.basepath, name)
      outdir = os.path.dirname(out)
      if not os.path.isdir(outdir):
        os.makedirs(outdir)
      return out

  class OutputHelper_symlink(OutputHelper_flat):
    '''Subclass of OutputHelper_flat that provides a helper for
    creating a symlink including creating the parent directories.
    '''
    def symlink(self, src, dest):
      out = self.ensureDirFor(dest)
      # remove previous link or file
      try:
        os.remove(out)
      except OSError, e:
        if e.errno != errno.ENOENT:
          raise
      os.symlink(src, out)

def main():
  jm = JarMaker()
  p = jm.getCommandLineParser()
  (options, args) = p.parse_args()
  jm.processIncludes(options.I)
  jm.outputFormat = options.f
  if options.e:
    jm.useChromeManifest = True
    jm.useJarfileManifest = False
  if options.bothManifests:
    jm.useChromeManifest = True
    jm.useJarfileManifest = True
  noise = logging.INFO
  if options.verbose is not None:
    noise = (options.verbose and logging.DEBUG) or logging.WARN
  if sys.version_info[:2] > (2,3):
    logging.basicConfig(format = "%(message)s")
  else:
    logging.basicConfig()
  logging.getLogger().setLevel(noise)
  topsrc = options.t
  topsrc = os.path.normpath(os.path.abspath(topsrc))
  if not args:
    jm.makeJar(infile=sys.stdin,
               sourcedirs=options.s, topsourcedir=topsrc,
               localedirs=options.l10n_src,
               jardir=options.j)
  else:
    jm.makeJars(args, options.l10n_base,
                jardir=options.j,
                sourcedirs=options.s, topsourcedir=topsrc,
                localedirs=options.l10n_src)

if __name__ == "__main__":
  main()
