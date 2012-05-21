# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

class MakeError(Exception):
    def __init__(self, message, loc=None):
        self.msg = message
        self.loc = loc

    def __str__(self):
        locstr = ''
        if self.loc is not None:
            locstr = "%s:" % (self.loc,)

        return "%s%s" % (locstr, self.msg)

def normaljoin(path, suffix):
    """
    Combine the given path with the suffix, and normalize if necessary to shrink the path to avoid hitting path length limits
    """
    result = os.path.join(path, suffix)
    if len(result) > 255:
        result = os.path.normpath(result)
    return result

def joiniter(fd, it):
    """
    Given an iterator that returns strings, write the words with a space in between each.
    """
    
    it = iter(it)
    for i in it:
        fd.write(i)
        break

    for i in it:
        fd.write(' ')
        fd.write(i)

def checkmsyscompat():
    """For msys compatibility on windows, honor the SHELL environment variable,
    and if $MSYSTEM == MINGW32, run commands through $SHELL -c instead of
    letting Python use the system shell."""
    if 'SHELL' in os.environ:
        shell = os.environ['SHELL']
    elif 'MOZILLABUILD' in os.environ:
        shell = os.environ['MOZILLABUILD'] + '/msys/bin/sh.exe'
    elif 'COMSPEC' in os.environ:
        shell = os.environ['COMSPEC']
    else:
        raise DataError("Can't find a suitable shell!")

    msys = False
    if 'MSYSTEM' in os.environ and os.environ['MSYSTEM'] == 'MINGW32':
        msys = True
        if not shell.lower().endswith(".exe"):
            shell += ".exe"
    return (shell, msys)

if hasattr(str, 'partition'):
    def strpartition(str, token):
        return str.partition(token)

    def strrpartition(str, token):
        return str.rpartition(token)

else:
    def strpartition(str, token):
        """Python 2.4 compatible str.partition"""

        offset = str.find(token)
        if offset == -1:
            return str, '', ''

        return str[:offset], token, str[offset + len(token):]

    def strrpartition(str, token):
        """Python 2.4 compatible str.rpartition"""

        offset = str.rfind(token)
        if offset == -1:
            return '', '', str

        return str[:offset], token, str[offset + len(token):]

try:
    from __builtin__ import any
except ImportError:
    def any(it):
        for i in it:
            if i:
                return True
        return False

class _MostUsedItem(object):
    __slots__ = ('key', 'o', 'count')

    def __init__(self, key):
        self.key = key
        self.o = None
        self.count = 1

    def __repr__(self):
        return "MostUsedItem(key=%r, count=%i, o=%r)" % (self.key, self.count, self.o)

class MostUsedCache(object):
    def __init__(self, capacity, creationfunc, verifyfunc):
        self.capacity = capacity
        self.cfunc = creationfunc
        self.vfunc = verifyfunc

        self.d = {}
        self.active = [] # lazily sorted!

    def setactive(self, item):
        if item in self.active:
            return

        if len(self.active) == self.capacity:
            self.active.sort(key=lambda i: i.count)
            old = self.active.pop(0)
            old.o = None
            # print "Evicting %s" % old.key

        self.active.append(item)

    def get(self, key):
        item = self.d.get(key, None)
        if item is None:
            item = _MostUsedItem(key)
            self.d[key] = item
        else:
            item.count += 1

        if item.o is not None and self.vfunc(key, item.o):
            return item.o

        item.o = self.cfunc(key)
        self.setactive(item)
        return item.o

    def verify(self):
        for k, v in self.d.iteritems():
            if v.o:
                assert v in self.active
            else:
                assert v not in self.active

    def debugitems(self):
        l = [i.key for i in self.active]
        l.sort()
        return l
