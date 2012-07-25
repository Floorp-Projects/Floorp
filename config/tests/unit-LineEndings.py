import unittest

from StringIO import StringIO
import os
import sys
import os.path
import mozunit

from Preprocessor import Preprocessor

class TestLineEndings(unittest.TestCase):
  """
  Unit tests for the Context class
  """

  def setUp(self):
    self.pp = Preprocessor()
    self.pp.out = StringIO()
    self.tempnam = os.tempnam('.')

  def tearDown(self):
    os.remove(self.tempnam)

  def createFile(self, lineendings):
    f = open(self.tempnam, 'wb')
    for line, ending in zip(['a', '#literal b', 'c'], lineendings):
      f.write(line+ending)
    f.close()

  def testMac(self):
    self.createFile(['\x0D']*3)
    self.pp.do_include(self.tempnam)
    self.assertEquals(self.pp.out.getvalue(), 'a\nb\nc\n')

  def testUnix(self):
    self.createFile(['\x0A']*3)
    self.pp.do_include(self.tempnam)
    self.assertEquals(self.pp.out.getvalue(), 'a\nb\nc\n')

  def testWindows(self):
    self.createFile(['\x0D\x0A']*3)
    self.pp.do_include(self.tempnam)
    self.assertEquals(self.pp.out.getvalue(), 'a\nb\nc\n')

if __name__ == '__main__':
  mozunit.main()
