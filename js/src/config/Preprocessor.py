"""
This is a very primitive line based preprocessor, for times when using
a C preprocessor isn't an option.
"""

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import sys
import os
import os.path
import re
from optparse import OptionParser
import errno

# hack around win32 mangling our line endings
# http://aspn.activestate.com/ASPN/Cookbook/Python/Recipe/65443
if sys.platform == "win32":
  import msvcrt
  msvcrt.setmode(sys.stdout.fileno(), os.O_BINARY)
  os.linesep = '\n'

import Expression

__all__ = ['Preprocessor', 'preprocess']


class Preprocessor:
  """
  Class for preprocessing text files.
  """
  class Error(RuntimeError):
    def __init__(self, cpp, MSG, context):
      self.file = cpp.context['FILE']
      self.line = cpp.context['LINE']
      self.key = MSG
      RuntimeError.__init__(self, (self.file, self.line, self.key, context))
  def __init__(self):
    self.context = Expression.Context()
    for k,v in {'FILE': '',
                'LINE': 0,
                'DIRECTORY': os.path.abspath('.')}.iteritems():
      self.context[k] = v
    self.actionLevel = 0
    self.disableLevel = 0
    # ifStates can be
    #  0: hadTrue
    #  1: wantsTrue
    #  2: #else found
    self.ifStates = []
    self.checkLineNumbers = False
    self.writtenLines = 0
    self.filters = []
    self.cmds = {}
    for cmd, level in {'define': 0,
                       'undef': 0,
                       'if': sys.maxint,
                       'ifdef': sys.maxint,
                       'ifndef': sys.maxint,
                       'else': 1,
                       'elif': 1,
                       'elifdef': 1,
                       'elifndef': 1,
                       'endif': sys.maxint,
                       'expand': 0,
                       'literal': 0,
                       'filter': 0,
                       'unfilter': 0,
                       'include': 0,
                       'includesubst': 0,
                       'error': 0}.iteritems():
      self.cmds[cmd] = (level, getattr(self, 'do_' + cmd))
    self.out = sys.stdout
    self.setMarker('#')
    self.LE = '\n'
    self.varsubst = re.compile('@(?P<VAR>\w+)@', re.U)
  
  def warnUnused(self, file):
    if self.actionLevel == 0:
      sys.stderr.write('{0}: WARNING: no preprocessor directives found\n'.format(file))
    elif self.actionLevel == 1:
      sys.stderr.write('{0}: WARNING: no useful preprocessor directives found\n'.format(file))
    pass

  def setLineEndings(self, aLE):
    """
    Set the line endings to be used for output.
    """
    self.LE = {'cr': '\x0D', 'lf': '\x0A', 'crlf': '\x0D\x0A'}[aLE]
  
  def setMarker(self, aMarker):
    """
    Set the marker to be used for processing directives.
    Used for handling CSS files, with pp.setMarker('%'), for example.
    The given marker may be None, in which case no markers are processed.
    """
    self.marker = aMarker
    if aMarker:
      self.instruction = re.compile('{0}(?P<cmd>[a-z]+)(?:\s(?P<args>.*))?$'
                                    .format(aMarker), 
                                    re.U)
      self.comment = re.compile(aMarker, re.U)
    else:
      class NoMatch(object):
        def match(self, *args):
          return False
      self.instruction = self.comment = NoMatch()
  
  def clone(self):
    """
    Create a clone of the current processor, including line ending
    settings, marker, variable definitions, output stream.
    """
    rv = Preprocessor()
    rv.context.update(self.context)
    rv.setMarker(self.marker)
    rv.LE = self.LE
    rv.out = self.out
    return rv
  
  def applyFilters(self, aLine):
    for f in self.filters:
      aLine = f[1](aLine)
    return aLine
  
  def write(self, aLine):
    """
    Internal method for handling output.
    """
    if self.checkLineNumbers:
      self.writtenLines += 1
      ln = self.context['LINE']
      if self.writtenLines != ln:
        self.out.write('//@line {line} "{file}"{le}'.format(line=ln,
                                                            file=self.context['FILE'],
                                                            le=self.LE))
        self.writtenLines = ln
    filteredLine = self.applyFilters(aLine)
    if filteredLine != aLine:
      self.actionLevel = 2
    # ensure our line ending. Only need to handle \n, as we're reading
    # with universal line ending support, at least for files.
    filteredLine = re.sub('\n', self.LE, filteredLine)
    self.out.write(filteredLine)
  
  def handleCommandLine(self, args, defaultToStdin = False):
    """
    Parse a commandline into this parser.
    Uses OptionParser internally, no args mean sys.argv[1:].
    """
    p = self.getCommandLineParser()
    (options, args) = p.parse_args(args=args)
    includes = options.I
    if options.output:
      dir = os.path.dirname(options.output)
      if dir and not os.path.exists(dir):
        try:
          os.makedirs(dir)
        except OSError as error:
          if error.errno != errno.EEXIST:
            raise
      self.out = open(options.output, 'w')
    if defaultToStdin and len(args) == 0:
      args = [sys.stdin]
    includes.extend(args)
    if includes:
      for f in includes:
        self.do_include(f, False)
      self.warnUnused(f)
    pass

  def getCommandLineParser(self, unescapeDefines = False):
    escapedValue = re.compile('".*"$')
    numberValue = re.compile('\d+$')
    def handleE(option, opt, value, parser):
      for k,v in os.environ.iteritems():
        self.context[k] = v
    def handleD(option, opt, value, parser):
      vals = value.split('=', 1)
      if len(vals) == 1:
        vals.append(1)
      elif unescapeDefines and escapedValue.match(vals[1]):
        # strip escaped string values
        vals[1] = vals[1][1:-1]
      elif numberValue.match(vals[1]):
        vals[1] = int(vals[1])
      self.context[vals[0]] = vals[1]
    def handleU(option, opt, value, parser):
      del self.context[value]
    def handleF(option, opt, value, parser):
      self.do_filter(value)
    def handleLE(option, opt, value, parser):
      self.setLineEndings(value)
    def handleMarker(option, opt, value, parser):
      self.setMarker(value)
    p = OptionParser()
    p.add_option('-I', action='append', type="string", default = [],
                 metavar="FILENAME", help='Include file')
    p.add_option('-E', action='callback', callback=handleE,
                 help='Import the environment into the defined variables')
    p.add_option('-D', action='callback', callback=handleD, type="string",
                 metavar="VAR[=VAL]", help='Define a variable')
    p.add_option('-U', action='callback', callback=handleU, type="string",
                 metavar="VAR", help='Undefine a variable')
    p.add_option('-F', action='callback', callback=handleF, type="string",
                 metavar="FILTER", help='Enable the specified filter')
    p.add_option('-o', '--output', type="string", default=None,
                 metavar="FILENAME", help='Output to the specified file '+
                 'instead of stdout')
    p.add_option('--line-endings', action='callback', callback=handleLE,
                 type="string", metavar="[cr|lr|crlf]",
                 help='Use the specified line endings [Default: OS dependent]')
    p.add_option('--marker', action='callback', callback=handleMarker,
                 type="string",
                 help='Use the specified marker instead of #')
    return p

  def handleLine(self, aLine):
    """
    Handle a single line of input (internal).
    """
    if self.actionLevel == 0 and self.comment.match(aLine):
      self.actionLevel = 1
    m = self.instruction.match(aLine)
    if m:
      args = None
      cmd = m.group('cmd')
      try:
        args = m.group('args')
      except IndexError:
        pass
      if cmd not in self.cmds:
        raise Preprocessor.Error(self, 'INVALID_CMD', aLine)
      level, cmd = self.cmds[cmd]
      if (level >= self.disableLevel):
        cmd(args)
      if cmd != 'literal':
        self.actionLevel = 2
    elif self.disableLevel == 0 and not self.comment.match(aLine):
      self.write(aLine)
    pass

  # Instruction handlers
  # These are named do_'instruction name' and take one argument
  
  # Variables
  def do_define(self, args):
    m = re.match('(?P<name>\w+)(?:\s(?P<value>.*))?', args, re.U)
    if not m:
      raise Preprocessor.Error(self, 'SYNTAX_DEF', args)
    val = 1
    if m.group('value'):
      val = self.applyFilters(m.group('value'))
      try:
        val = int(val)
      except:
        pass
    self.context[m.group('name')] = val
  def do_undef(self, args):
    m = re.match('(?P<name>\w+)$', args, re.U)
    if not m:
      raise Preprocessor.Error(self, 'SYNTAX_DEF', args)
    if args in self.context:
      del self.context[args]
  # Logic
  def ensure_not_else(self):
    if len(self.ifStates) == 0 or self.ifStates[-1] == 2:
      sys.stderr.write('WARNING: bad nesting of #else\n')
  def do_if(self, args, replace=False):
    if self.disableLevel and not replace:
      self.disableLevel += 1
      return
    val = None
    try:
      e = Expression.Expression(args)
      val = e.evaluate(self.context)
    except Exception:
      # XXX do real error reporting
      raise Preprocessor.Error(self, 'SYNTAX_ERR', args)
    if type(val) == str:
      # we're looking for a number value, strings are false
      val = False
    if not val:
      self.disableLevel = 1
    if replace:
      if val:
        self.disableLevel = 0
      self.ifStates[-1] = self.disableLevel
    else:
      self.ifStates.append(self.disableLevel)
    pass
  def do_ifdef(self, args, replace=False):
    if self.disableLevel and not replace:
      self.disableLevel += 1
      return
    if re.match('\W', args, re.U):
      raise Preprocessor.Error(self, 'INVALID_VAR', args)
    if args not in self.context:
      self.disableLevel = 1
    if replace:
      if args in self.context:
        self.disableLevel = 0
      self.ifStates[-1] = self.disableLevel
    else:
      self.ifStates.append(self.disableLevel)
    pass
  def do_ifndef(self, args, replace=False):
    if self.disableLevel and not replace:
      self.disableLevel += 1
      return
    if re.match('\W', args, re.U):
      raise Preprocessor.Error(self, 'INVALID_VAR', args)
    if args in self.context:
      self.disableLevel = 1
    if replace:
      if args not in self.context:
        self.disableLevel = 0
      self.ifStates[-1] = self.disableLevel
    else:
      self.ifStates.append(self.disableLevel)
    pass
  def do_else(self, args, ifState = 2):
    self.ensure_not_else()
    hadTrue = self.ifStates[-1] == 0
    self.ifStates[-1] = ifState # in-else
    if hadTrue:
      self.disableLevel = 1
      return
    self.disableLevel = 0
  def do_elif(self, args):
    if self.disableLevel == 1:
      if self.ifStates[-1] == 1:
        self.do_if(args, replace=True)
    else:
      self.do_else(None, self.ifStates[-1])
  def do_elifdef(self, args):
    if self.disableLevel == 1:
      if self.ifStates[-1] == 1:
        self.do_ifdef(args, replace=True)
    else:
      self.do_else(None, self.ifStates[-1])
  def do_elifndef(self, args):
    if self.disableLevel == 1:
      if self.ifStates[-1] == 1:
        self.do_ifndef(args, replace=True)
    else:
      self.do_else(None, self.ifStates[-1])
  def do_endif(self, args):
    if self.disableLevel > 0:
      self.disableLevel -= 1
    if self.disableLevel == 0:
      self.ifStates.pop()
  # output processing
  def do_expand(self, args):
    lst = re.split('__(\w+)__', args, re.U)
    do_replace = False
    def vsubst(v):
      if v in self.context:
        return str(self.context[v])
      return ''
    for i in range(1, len(lst), 2):
      lst[i] = vsubst(lst[i])
    lst.append('\n') # add back the newline
    self.write(reduce(lambda x, y: x+y, lst, ''))
  def do_literal(self, args):
    self.write(args + self.LE)
  def do_filter(self, args):
    filters = [f for f in args.split(' ') if hasattr(self, 'filter_' + f)]
    if len(filters) == 0:
      return
    current = dict(self.filters)
    for f in filters:
      current[f] = getattr(self, 'filter_' + f)
    filterNames = current.keys()
    filterNames.sort()
    self.filters = [(fn, current[fn]) for fn in filterNames]
    return
  def do_unfilter(self, args):
    filters = args.split(' ')
    current = dict(self.filters)
    for f in filters:
      if f in current:
        del current[f]
    filterNames = current.keys()
    filterNames.sort()
    self.filters = [(fn, current[fn]) for fn in filterNames]
    return
  # Filters
  #
  # emptyLines
  #   Strips blank lines from the output.
  def filter_emptyLines(self, aLine):
    if aLine == '\n':
      return ''
    return aLine
  # slashslash
  #   Strips everything after //
  def filter_slashslash(self, aLine):
    if (aLine.find('//') == -1):
      return aLine
    [aLine, rest] = aLine.split('//', 1)
    if rest:
      aLine += '\n'
    return aLine
  # spaces
  #   Collapses sequences of spaces into a single space
  def filter_spaces(self, aLine):
    return re.sub(' +', ' ', aLine).strip(' ')
  # substition
  #   helper to be used by both substition and attemptSubstitution
  def filter_substitution(self, aLine, fatal=True):
    def repl(matchobj):
      varname = matchobj.group('VAR')
      if varname in self.context:
        return str(self.context[varname])
      if fatal:
        raise Preprocessor.Error(self, 'UNDEFINED_VAR', varname)
      return matchobj.group(0)
    return self.varsubst.sub(repl, aLine)
  def filter_attemptSubstitution(self, aLine):
    return self.filter_substitution(aLine, fatal=False)
  # File ops
  def do_include(self, args, filters=True):
    """
    Preprocess a given file.
    args can either be a file name, or a file-like object.
    Files should be opened, and will be closed after processing.
    """
    isName = type(args) == str or type(args) == unicode
    oldWrittenLines = self.writtenLines
    oldCheckLineNumbers = self.checkLineNumbers
    self.checkLineNumbers = False
    if isName:
      try:
        args = str(args)
        if filters:
          args = self.applyFilters(args)
        if not os.path.isabs(args):
          args = os.path.join(self.context['DIRECTORY'], args)
        args = open(args, 'rU')
      except Preprocessor.Error:
        raise
      except:
        raise Preprocessor.Error(self, 'FILE_NOT_FOUND', str(args))
    self.checkLineNumbers = bool(re.search('\.(js|jsm|java)(?:\.in)?$', args.name))
    oldFile = self.context['FILE']
    oldLine = self.context['LINE']
    oldDir = self.context['DIRECTORY']
    if args.isatty():
      # we're stdin, use '-' and '' for file and dir
      self.context['FILE'] = '-'
      self.context['DIRECTORY'] = ''
    else:
      abspath = os.path.abspath(args.name)
      self.context['FILE'] = abspath
      self.context['DIRECTORY'] = os.path.dirname(abspath)
    self.context['LINE'] = 0
    self.writtenLines = 0
    for l in args:
      self.context['LINE'] += 1
      self.handleLine(l)
    args.close()
    self.context['FILE'] = oldFile
    self.checkLineNumbers = oldCheckLineNumbers
    self.writtenLines = oldWrittenLines
    self.context['LINE'] = oldLine
    self.context['DIRECTORY'] = oldDir
  def do_includesubst(self, args):
    args = self.filter_substitution(args)
    self.do_include(args)
  def do_error(self, args):
    raise Preprocessor.Error(self, 'Error: ', str(args))

def main():
  pp = Preprocessor()
  pp.handleCommandLine(None, True)
  return

def preprocess(includes=[sys.stdin], defines={},
               output = sys.stdout,
               line_endings='\n', marker='#'):
  pp = Preprocessor()
  pp.context.update(defines)
  pp.setLineEndings(line_endings)
  pp.setMarker(marker)
  pp.out = output
  for f in includes:
    pp.do_include(f, False)

if __name__ == "__main__":
  main()
