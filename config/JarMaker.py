# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
if sys.platform == "win32":
  from ctypes import windll, WinError
  CreateHardLink = windll.kernel32.CreateHardLinkA

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
  relsrcline = re.compile('relativesrcdir\s+(?P<relativesrcdir>.+?):')
  regline = re.compile('\%\s+(.*)$')
  entryre = '(?P<optPreprocess>\*)?(?P<optOverwrite>\+?)\s+'
  entryline = re.compile(entryre + '(?P<output>[\w\d.\-\_\\\/\+\@]+)\s*(\((?P<locale>\%?)(?P<source>[\w\d.\-\_\\\/\@]+)\))?\s*$')

  def __init__(self, outputFormat = 'flat', useJarfileManifest = True,
               useChromeManifest = False):
    self.outputFormat = outputFormat
    self.useJarfileManifest = useJarfileManifest
    self.useChromeManifest = useChromeManifest
    self.pp = Preprocessor()
    self.topsourcedir = None
    self.sourcedirs = []
    self.localedirs = None
    self.l10nbase = None
    self.l10nmerge = None
    self.relativesrcdir = None
    self.rootManifestAppId = None

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
    p.add_option('--l10n-base', type="string", action="store",
                 help="base directory to be used for localization (requires relativesrcdir)")
    p.add_option('--locale-mergedir', type="string", action="store",
                 help="base directory to be used for l10n-merge (requires l10n-base and relativesrcdir)")
    p.add_option('--relativesrcdir', type="string",
                 help="relativesrcdir to be used for localization")
    p.add_option('-j', type="string",
                 help="jarfile directory")
    p.add_option('--root-manifest-entry-appid', type="string",
                 help="add an app id specific root chrome manifest entry.")
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
      self.updateManifest(jarPath + '.manifest', chromebasepath.format(''),
                          register)
      addEntriesToListFile(chromeManifest, ['manifest chrome/{0}.manifest'
                                            .format(os.path.basename(jarPath))])
    if self.useChromeManifest:
      self.updateManifest(chromeManifest, chromebasepath.format('chrome/'),
                          register)

    # If requested, add a root chrome manifest entry (assumed to be in the parent directory
    # of chromeManifest) with the application specific id. In cases where we're building
    # lang packs, the root manifest must know about application sub directories.
    if self.rootManifestAppId:
      rootChromeManifest = os.path.join(os.path.normpath(os.path.dirname(chromeManifest)),
                                        '..', 'chrome.manifest')
      rootChromeManifest = os.path.normpath(rootChromeManifest)
      chromeDir = os.path.basename(os.path.dirname(os.path.normpath(chromeManifest)))
      logging.info("adding '%s' entry to root chrome manifest appid=%s" % (chromeDir, self.rootManifestAppId))
      addEntriesToListFile(rootChromeManifest, ['manifest %s/chrome.manifest application=%s' % (chromeDir, self.rootManifestAppId)])

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

  def makeJar(self, infile, jardir):
    '''makeJar is the main entry point to JarMaker.

    It takes the input file, the output directory, the source dirs and the
    top source dir as argument, and optionally the l10n dirs.
    '''
    # making paths absolute, guess srcdir if file and add to sourcedirs
    _normpath = lambda p: os.path.normpath(os.path.abspath(p))
    self.topsourcedir = _normpath(self.topsourcedir)
    self.sourcedirs = [_normpath(p) for p in self.sourcedirs]
    if self.localedirs:
      self.localedirs = [_normpath(p) for p in self.localedirs]
    elif self.relativesrcdir:
      self.localedirs = self.generateLocaleDirs(self.relativesrcdir)
    if isinstance(infile, basestring):
      logging.info("processing " + infile)
      self.sourcedirs.append(_normpath(os.path.dirname(infile)))
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
        self.processJarSection(m.group('jarfile'), lines, jardir)
    except StopIteration:
      # we read the file
      pass
    return

  def generateLocaleDirs(self, relativesrcdir):
    if os.path.basename(relativesrcdir) == 'locales':
      # strip locales
      l10nrelsrcdir = os.path.dirname(relativesrcdir)
    else:
      l10nrelsrcdir = relativesrcdir
    locdirs = []
    # generate locales dirs, merge, l10nbase, en-US
    if self.l10nmerge:
      locdirs.append(os.path.join(self.l10nmerge, l10nrelsrcdir))
    if self.l10nbase:
      locdirs.append(os.path.join(self.l10nbase, l10nrelsrcdir))
    if self.l10nmerge or not self.l10nbase:
      # add en-US if we merge, or if it's not l10n
      locdirs.append(os.path.join(self.topsourcedir, relativesrcdir, 'en-US'))
    return locdirs

  def processJarSection(self, jarfile, lines, jardir):
    '''Internal method called by makeJar to actually process a section
    of a jar.mn file.

    jarfile is the basename of the jarfile or the directory name for 
    flat output, lines is a pushback_iterator of the lines of jar.mn,
    the remaining options are carried over from makeJar.
    '''

    # chromebasepath is used for chrome registration manifests
    # {0} is getting replaced with chrome/ for chrome.manifest, and with
    # an empty string for jarfile.manifest
    chromebasepath = '{0}' + os.path.basename(jarfile)
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
      except OSError as error:
        if error.errno != errno.EEXIST:
          raise
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
        m = self.relsrcline.match(l)
        if m:
          relativesrcdir = m.group('relativesrcdir')
          self.localedirs = self.generateLocaleDirs(relativesrcdir)
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
        self._processEntryLine(m, outHelper, jf)
    finally:
      if jf is not None:
        jf.close()
    return

  def _processEntryLine(self, m, outHelper, jf):
      out = m.group('output')
      src = m.group('source') or os.path.basename(out)
      # pick the right sourcedir -- l10n, topsrc or src
      if m.group('locale'):
        src_base = self.localedirs
      elif src.startswith('/'):
        # path/in/jar/file_name.xul     (/path/in/sourcetree/file_name.xul)
        # refers to a path relative to topsourcedir, use that as base
        # and strip the leading '/'
        src_base = [self.topsourcedir]
        src = src[1:]
      else:
        # use srcdirs and the objdir (current working dir) for relative paths
        src_base = self.sourcedirs + [os.getcwd()]
      # check if the source file exists
      realsrc = None
      for _srcdir in src_base:
        if os.path.isfile(os.path.join(_srcdir, src)):
          realsrc = os.path.join(_srcdir, src)
          break
      if realsrc is None:
        if jf is not None:
          jf.close()
        raise RuntimeError('File "{0}" not found in {1}'
                           .format(src, ', '.join(src_base)))
      if m.group('optPreprocess'):
        outf = outHelper.getOutput(out)
        inf = open(realsrc)
        pp = self.pp.clone()
        if src[-4:] == '.css':
          pp.setMarker('%')
        pp.out = outf
        pp.do_include(inf)
        pp.warnUnused(realsrc)
        outf.close()
        inf.close()
        return
      # copy or symlink if newer or overwrite
      if (m.group('optOverwrite')
          or (getModTime(realsrc) >
              outHelper.getDestModTime(m.group('output')))):
        if self.outputFormat == 'symlink':
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
      except OSError as e:
        if e.errno != errno.ENOENT:
          raise
      return open(out, 'wb')
    def ensureDirFor(self, name):
      out = os.path.join(self.basepath, name)
      outdir = os.path.dirname(out)
      if not os.path.isdir(outdir):
        try:
          os.makedirs(outdir)
        except OSError as error:
          if error.errno != errno.EEXIST:
            raise
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
      except OSError as e:
        if e.errno != errno.ENOENT:
          raise
      if sys.platform != "win32":
        os.symlink(src, out)
      else:
        # On Win32, use ctypes to create a hardlink
        rv = CreateHardLink(out, src, None)
        if rv == 0:
          raise WinError()

def main():
  jm = JarMaker()
  p = jm.getCommandLineParser()
  (options, args) = p.parse_args()
  jm.processIncludes(options.I)
  jm.outputFormat = options.f
  jm.sourcedirs = options.s
  jm.topsourcedir = options.t
  if options.e:
    jm.useChromeManifest = True
    jm.useJarfileManifest = False
  if options.bothManifests:
    jm.useChromeManifest = True
    jm.useJarfileManifest = True
  if options.l10n_base:
    if not options.relativesrcdir:
      p.error('relativesrcdir required when using l10n-base')
    if options.l10n_src:
      p.error('both l10n-src and l10n-base are not supported')
    jm.l10nbase = options.l10n_base
    jm.relativesrcdir = options.relativesrcdir
    jm.l10nmerge = options.locale_mergedir
  elif options.locale_mergedir:
    p.error('l10n-base required when using locale-mergedir')
  jm.localedirs = options.l10n_src
  if options.root_manifest_entry_appid:
    jm.rootManifestAppId = options.root_manifest_entry_appid
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
    infile = sys.stdin
  else:
    infile,  = args
  jm.makeJar(infile, options.j)

if __name__ == "__main__":
  main()
