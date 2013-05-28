#!/usr/bin/env python
import unittest
import json, os, sys, time, tempfile
from StringIO import StringIO
import mozunit

from writemozinfo import build_dict, write_json

class TestBuildDict(unittest.TestCase):
    def testMissing(self):
        """
        Test that missing required values raises.
        """
        self.assertRaises(Exception, build_dict, {'OS_TARGET':'foo'})
        self.assertRaises(Exception, build_dict, {'TARGET_CPU':'foo'})
        self.assertRaises(Exception, build_dict, {'MOZ_WIDGET_TOOLKIT':'foo'})

    def testWin(self):
        d = build_dict({'OS_TARGET':'WINNT',
                        'TARGET_CPU':'i386',
                        'MOZ_WIDGET_TOOLKIT':'windows'})
        self.assertEqual('win', d['os'])
        self.assertEqual('x86', d['processor'])
        self.assertEqual('windows', d['toolkit'])
        self.assertEqual(32, d['bits'])

    def testLinux(self):
        d = build_dict({'OS_TARGET':'Linux',
                        'TARGET_CPU':'i386',
                        'MOZ_WIDGET_TOOLKIT':'gtk2'})
        self.assertEqual('linux', d['os'])
        self.assertEqual('x86', d['processor'])
        self.assertEqual('gtk2', d['toolkit'])
        self.assertEqual(32, d['bits'])

        d = build_dict({'OS_TARGET':'Linux',
                        'TARGET_CPU':'x86_64',
                        'MOZ_WIDGET_TOOLKIT':'gtk2'})
        self.assertEqual('linux', d['os'])
        self.assertEqual('x86_64', d['processor'])
        self.assertEqual('gtk2', d['toolkit'])
        self.assertEqual(64, d['bits'])

    def testMac(self):
        d = build_dict({'OS_TARGET':'Darwin',
                        'TARGET_CPU':'i386',
                        'MOZ_WIDGET_TOOLKIT':'cocoa'})
        self.assertEqual('mac', d['os'])
        self.assertEqual('x86', d['processor'])
        self.assertEqual('cocoa', d['toolkit'])
        self.assertEqual(32, d['bits'])

        d = build_dict({'OS_TARGET':'Darwin',
                        'TARGET_CPU':'x86_64',
                        'MOZ_WIDGET_TOOLKIT':'cocoa'})
        self.assertEqual('mac', d['os'])
        self.assertEqual('x86_64', d['processor'])
        self.assertEqual('cocoa', d['toolkit'])
        self.assertEqual(64, d['bits'])

    def testMacUniversal(self):
        d = build_dict({'OS_TARGET':'Darwin',
                        'TARGET_CPU':'i386',
                        'MOZ_WIDGET_TOOLKIT':'cocoa',
                        'UNIVERSAL_BINARY': '1'})
        self.assertEqual('mac', d['os'])
        self.assertEqual('universal-x86-x86_64', d['processor'])
        self.assertEqual('cocoa', d['toolkit'])
        self.assertFalse('bits' in d)

        d = build_dict({'OS_TARGET':'Darwin',
                        'TARGET_CPU':'x86_64',
                        'MOZ_WIDGET_TOOLKIT':'cocoa',
                        'UNIVERSAL_BINARY': '1'})
        self.assertEqual('mac', d['os'])
        self.assertEqual('universal-x86-x86_64', d['processor'])
        self.assertEqual('cocoa', d['toolkit'])
        self.assertFalse('bits' in d)

    def testAndroid(self):
        d = build_dict({'OS_TARGET':'Android',
                        'TARGET_CPU':'arm',
                        'MOZ_WIDGET_TOOLKIT':'android'})
        self.assertEqual('android', d['os'])
        self.assertEqual('arm', d['processor'])
        self.assertEqual('android', d['toolkit'])
        self.assertEqual(32, d['bits'])

    def testX86(self):
        """
        Test that various i?86 values => x86.
        """
        d = build_dict({'OS_TARGET':'WINNT',
                        'TARGET_CPU':'i486',
                        'MOZ_WIDGET_TOOLKIT':'windows'})
        self.assertEqual('x86', d['processor'])

        d = build_dict({'OS_TARGET':'WINNT',
                        'TARGET_CPU':'i686',
                        'MOZ_WIDGET_TOOLKIT':'windows'})
        self.assertEqual('x86', d['processor'])

    def testARM(self):
        """
        Test that all arm CPU architectures => arm.
        """
        d = build_dict({'OS_TARGET':'Linux',
                        'TARGET_CPU':'arm',
                        'MOZ_WIDGET_TOOLKIT':'gtk2'})
        self.assertEqual('arm', d['processor'])

        d = build_dict({'OS_TARGET':'Linux',
                        'TARGET_CPU':'armv7',
                        'MOZ_WIDGET_TOOLKIT':'gtk2'})
        self.assertEqual('arm', d['processor'])

    def testUnknown(self):
        """
        Test that unknown values pass through okay.
        """
        d = build_dict({'OS_TARGET':'RandOS',
                        'TARGET_CPU':'cptwo',
                        'MOZ_WIDGET_TOOLKIT':'foobar'})
        self.assertEqual("randos", d["os"])
        self.assertEqual("cptwo", d["processor"])
        self.assertEqual("foobar", d["toolkit"])
        # unknown CPUs should not get a bits value
        self.assertFalse("bits" in d)
        
    def testDebug(self):
        """
        Test that debug values are properly detected.
        """
        d = build_dict({'OS_TARGET':'Linux',
                        'TARGET_CPU':'i386',
                        'MOZ_WIDGET_TOOLKIT':'gtk2'})
        self.assertEqual(False, d['debug'])
        
        d = build_dict({'OS_TARGET':'Linux',
                        'TARGET_CPU':'i386',
                        'MOZ_WIDGET_TOOLKIT':'gtk2',
                        'MOZ_DEBUG':'1'})
        self.assertEqual(True, d['debug'])

    def testCrashreporter(self):
        """
        Test that crashreporter values are properly detected.
        """
        d = build_dict({'OS_TARGET':'Linux',
                        'TARGET_CPU':'i386',
                        'MOZ_WIDGET_TOOLKIT':'gtk2'})
        self.assertEqual(False, d['crashreporter'])
        
        d = build_dict({'OS_TARGET':'Linux',
                        'TARGET_CPU':'i386',
                        'MOZ_WIDGET_TOOLKIT':'gtk2',
                        'MOZ_CRASHREPORTER':'1'})
        self.assertEqual(True, d['crashreporter'])

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
                                'TARGET_CPU':'i386',
                                'TOPSRCDIR':'/tmp',
                                'MOZCONFIG':'foo',
                                'MOZ_WIDGET_TOOLKIT':'windows'})
        with open(self.f) as f:
            d = json.load(f)
            self.assertEqual('win', d['os'])
            self.assertEqual('x86', d['processor'])
            self.assertEqual('windows', d['toolkit'])
            self.assertEqual('/tmp', d['topsrcdir'])
            self.assertEqual(os.path.normpath('/tmp/foo'), d['mozconfig'])
            self.assertEqual(32, d['bits'])

    def testFileObj(self):
        """
        Test that writing to a file-like object produces correct output.
        """
        s = StringIO()
        write_json(s, env={'OS_TARGET':'WINNT',
                           'TARGET_CPU':'i386',
                           'MOZ_WIDGET_TOOLKIT':'windows'})
        d = json.loads(s.getvalue())
        self.assertEqual('win', d['os'])
        self.assertEqual('x86', d['processor'])
        self.assertEqual('windows', d['toolkit'])
        self.assertEqual(32, d['bits'])

if __name__ == '__main__':
    mozunit.main()
