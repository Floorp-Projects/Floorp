#!/usr/bin/env python
from __future__ import with_statement
import unittest
import os, sys, time, tempfile
from StringIO import StringIO

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))

from writemozinfo import build_dict, write_json, JsonValue, jsonify

class TestBuildDict(unittest.TestCase):
    def testMissing(self):
        """
        Test that missing required values raises.
        """
        self.assertRaises(Exception, build_dict, {})
        self.assertRaises(Exception, build_dict, {'OS_TARGET':'foo'})
        self.assertRaises(Exception, build_dict, {'TARGET_CPU':'foo'})

    def testWin(self):
        d = build_dict({'OS_TARGET':'WINNT',
                        'TARGET_CPU':'i386'})
        self.assertEqual('win', d['os'])
        self.assertEqual('x86', d['processor'])
        self.assertEqual(32, d['bits'])

    def testLinux(self):
        d = build_dict({'OS_TARGET':'Linux',
                        'TARGET_CPU':'i386'})
        self.assertEqual('linux', d['os'])
        self.assertEqual('x86', d['processor'])
        self.assertEqual(32, d['bits'])

        d = build_dict({'OS_TARGET':'Linux',
                        'TARGET_CPU':'x86_64'})
        self.assertEqual('linux', d['os'])
        self.assertEqual('x86_64', d['processor'])
        self.assertEqual(64, d['bits'])

    def testMac(self):
        d = build_dict({'OS_TARGET':'Darwin',
                        'TARGET_CPU':'i386'})
        self.assertEqual('mac', d['os'])
        self.assertEqual('x86', d['processor'])
        self.assertEqual(32, d['bits'])

        d = build_dict({'OS_TARGET':'Darwin',
                        'TARGET_CPU':'x86_64'})
        self.assertEqual('mac', d['os'])
        self.assertEqual('x86_64', d['processor'])
        self.assertEqual(64, d['bits'])

    def testAndroid(self):
        d = build_dict({'OS_TARGET':'Android',
                        'TARGET_CPU':'arm'})
        self.assertEqual('android', d['os'])
        self.assertEqual('arm', d['processor'])
        self.assertEqual(32, d['bits'])

    def testX86(self):
        """
        Test that various i?86 values => x86.
        """
        d = build_dict({'OS_TARGET':'WINNT',
                        'TARGET_CPU':'i486'})
        self.assertEqual('x86', d['processor'])

        d = build_dict({'OS_TARGET':'WINNT',
                        'TARGET_CPU':'i686'})
        self.assertEqual('x86', d['processor'])

    def testARM(self):
        """
        Test that all arm CPU architectures => arm.
        """
        d = build_dict({'OS_TARGET':'Linux',
                        'TARGET_CPU':'arm'})
        self.assertEqual('arm', d['processor'])

        d = build_dict({'OS_TARGET':'Linux',
                        'TARGET_CPU':'armv7'})
        self.assertEqual('arm', d['processor'])

    def testUnknown(self):
        """
        Test that unknown values pass through okay.
        """
        d = build_dict({'OS_TARGET':'RandOS',
                        'TARGET_CPU':'cptwo'})
        self.assertEqual("randos", d["os"])
        self.assertEqual("cptwo", d["processor"])
        # unknown CPUs should not get a bits value
        self.assertFalse("bits" in d)
        
    def testDebug(self):
        """
        Test that debug values are properly detected.
        """
        d = build_dict({'OS_TARGET':'Linux',
                        'TARGET_CPU':'i386'})
        self.assertEqual(False, d['debug'])
        
        d = build_dict({'OS_TARGET':'Linux',
                        'TARGET_CPU':'i386',
                        'MOZ_DEBUG':'1'})
        self.assertEqual(True, d['debug'])

class TestJsonValue(unittest.TestCase):
    def testNone(self):
        self.assertEqual("null", repr(JsonValue(None)))
        
    def testBool(self):
        self.assertEqual("true", repr(JsonValue(True)))
        self.assertEqual("false", repr(JsonValue(False)))

    def testStr(self):
        self.assertEqual("'abc'", repr(JsonValue("abc")))

    def testInt(self):
        self.assertEqual("100", repr(JsonValue(100)))

    def testInvalid(self):
        self.assertRaises(Exception, JsonValue, unicode("abc"))
        self.assertRaises(Exception, JsonValue, 123.45)

def parse_json(j):
    """
    Awful hack to parse a restricted subset of JSON strings into Python dicts.
    """
    return eval(j, {'true':True,'false':False,'null':None})

class TestJsonify(unittest.TestCase):
    """
    Test the jsonify function.
    """
    def testBasic(self):
        """
        Sanity check the set of accepted Python value types.
        """
        j = parse_json(jsonify({'a':True,'b':False,'c':None,'d':100,'e':"abc"}))
        self.assertEquals(True, j['a'])
        self.assertEquals(False, j['b'])
        self.assertEquals(None, j['c'])
        self.assertEquals(100, j['d'])
        self.assertEquals("abc", j['e'])

class TestWriteJson(unittest.TestCase):
    """
    Test the write_json function.
    """
    def setUp(self):
        fd, self.f = tempfile.mkstemp()
        os.close(fd)

    def tearDown(self):
        os.unlink(self.f)

    def testBasic(self):
        """
        Test that writing to a file produces correct output.
        """
        write_json(self.f, env={'OS_TARGET':'WINNT',
                                'TARGET_CPU':'i386'})
        with open(self.f) as f:
            d = parse_json(f.read())
            self.assertEqual('win', d['os'])
            self.assertEqual('x86', d['processor'])
            self.assertEqual(32, d['bits'])

    def testFileObj(self):
        """
        Test that writing to a file-like object produces correct output.
        """
        s = StringIO()
        write_json(s, env={'OS_TARGET':'WINNT',
                           'TARGET_CPU':'i386'})
        d = parse_json(s.getvalue())
        self.assertEqual('win', d['os'])
        self.assertEqual('x86', d['processor'])
        self.assertEqual(32, d['bits'])

if __name__ == '__main__':
    unittest.main()
  
