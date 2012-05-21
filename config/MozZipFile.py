# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import zipfile
import time
import binascii, struct
import zlib
import os
from utils import lockFile

class ZipFile(zipfile.ZipFile):
  """ Class with methods to open, read, write, close, list zip files.

  Subclassing zipfile.ZipFile to allow for overwriting of existing
  entries, though only for writestr, not for write.
  """
  def __init__(self, file, mode="r", compression=zipfile.ZIP_STORED,
               lock = False):
    if lock:
      assert isinstance(file, basestring)
      self.lockfile = lockFile(file + '.lck')
    else:
      self.lockfile = None

    if mode == 'a' and lock:
      # appending to a file which doesn't exist fails, but we can't check
      # existence util we hold the lock
      if (not os.path.isfile(file)) or os.path.getsize(file) == 0:
        mode = 'w'

    zipfile.ZipFile.__init__(self, file, mode, compression)
    self._remove = []
    self.end = self.fp.tell()
    self.debug = 0

  def writestr(self, zinfo_or_arcname, bytes):
    """Write contents into the archive.

    The contents is the argument 'bytes',  'zinfo_or_arcname' is either
    a ZipInfo instance or the name of the file in the archive.
    This method is overloaded to allow overwriting existing entries.
    """
    if not isinstance(zinfo_or_arcname, zipfile.ZipInfo):
      zinfo = zipfile.ZipInfo(filename=zinfo_or_arcname,
                              date_time=time.localtime(time.time()))
      zinfo.compress_type = self.compression
      # Add some standard UNIX file access permissions (-rw-r--r--).
      zinfo.external_attr = (0x81a4 & 0xFFFF) << 16L
    else:
      zinfo = zinfo_or_arcname

    # Now to the point why we overwrote this in the first place,
    # remember the entry numbers if we already had this entry.
    # Optimizations:
    # If the entry to overwrite is the last one, just reuse that.
    # If we store uncompressed and the new content has the same size
    # as the old, reuse the existing entry.

    doSeek = False # store if we need to seek to the eof after overwriting
    if self.NameToInfo.has_key(zinfo.filename):
      # Find the last ZipInfo with our name.
      # Last, because that's catching multiple overwrites
      i = len(self.filelist)
      while i > 0:
        i -= 1
        if self.filelist[i].filename == zinfo.filename:
          break
      zi = self.filelist[i]
      if ((zinfo.compress_type == zipfile.ZIP_STORED
           and zi.compress_size == len(bytes))
          or (i + 1) == len(self.filelist)):
        # make sure we're allowed to write, otherwise done by writestr below
        self._writecheck(zi)
        # overwrite existing entry
        self.fp.seek(zi.header_offset)
        if (i + 1) == len(self.filelist):
          # this is the last item in the file, just truncate
          self.fp.truncate()
        else:
          # we need to move to the end of the file afterwards again
          doSeek = True
        # unhook the current zipinfo, the writestr of our superclass
        # will add a new one
        self.filelist.pop(i)
        self.NameToInfo.pop(zinfo.filename)
      else:
        # Couldn't optimize, sadly, just remember the old entry for removal
        self._remove.append(self.filelist.pop(i))
    zipfile.ZipFile.writestr(self, zinfo, bytes)
    self.filelist.sort(lambda l, r: cmp(l.header_offset, r.header_offset))
    if doSeek:
      self.fp.seek(self.end)
    self.end = self.fp.tell()

  def close(self):
    """Close the file, and for mode "w" and "a" write the ending
    records.

    Overwritten to compact overwritten entries.
    """
    if not self._remove:
      # we don't have anything special to do, let's just call base
      r = zipfile.ZipFile.close(self)
      self.lockfile = None
      return r

    if self.fp.mode != 'r+b':
      # adjust file mode if we originally just wrote, now we rewrite
      self.fp.close()
      self.fp = open(self.filename, 'r+b')
    all = map(lambda zi: (zi, True), self.filelist) + \
        map(lambda zi: (zi, False), self._remove)
    all.sort(lambda l, r: cmp(l[0].header_offset, r[0].header_offset))
    # empty _remove for multiple closes
    self._remove = []

    lengths = [all[i+1][0].header_offset - all[i][0].header_offset
               for i in xrange(len(all)-1)]
    lengths.append(self.end - all[-1][0].header_offset)
    to_pos = 0
    for (zi, keep), length in zip(all, lengths):
      if not keep:
        continue
      oldoff = zi.header_offset
      # python <= 2.4 has file_offset
      if hasattr(zi, 'file_offset'):
        zi.file_offset = zi.file_offset + to_pos - oldoff
      zi.header_offset = to_pos
      self.fp.seek(oldoff)
      content = self.fp.read(length)
      self.fp.seek(to_pos)
      self.fp.write(content)
      to_pos += length
    self.fp.truncate()
    zipfile.ZipFile.close(self)
    self.lockfile = None
