import unittest

import mozunit

from printprereleasesuffix import get_prerelease_suffix


class TestGetPreReleaseSuffix(unittest.TestCase):
    """
    Unit tests for the get_prerelease_suffix function
    """

    def test_alpha_1(self):
        """test 1a1 version string"""
        self.c = get_prerelease_suffix('1a1')
        self.assertEqual(self.c, ' 1 Alpha 1')

    def test_alpha_10(self):
        """test 1.2a10 version string"""
        self.c = get_prerelease_suffix('1.2a10')
        self.assertEqual(self.c, ' 1.2 Alpha 10')

    def test_beta_3(self):
        """test 1.2.3b3 version string"""
        self.c = get_prerelease_suffix('1.2.3b3')
        self.assertEqual(self.c, ' 1.2.3 Beta 3')

    def test_beta_30(self):
        """test 1.2.3.4b30 version string"""
        self.c = get_prerelease_suffix('1.2.3.4b30')
        self.assertEqual(self.c, ' 1.2.3.4 Beta 30')

    def test_release_1(self):
        """test 1.2.3.4 version string"""
        self.c = get_prerelease_suffix('1.2.3.4')
        self.assertEqual(self.c, '')

    def test_alpha_1_pre(self):
        """test 1.2a1pre version string"""
        self.c = get_prerelease_suffix('1.2a1pre')
        self.assertEqual(self.c, '')

    def test_beta_10_pre(self):
        """test 3.4b10pre version string"""
        self.c = get_prerelease_suffix('3.4b10pre')
        self.assertEqual(self.c, '')

    def test_pre_0(self):
        """test 1.2pre0 version string"""
        self.c = get_prerelease_suffix('1.2pre0')
        self.assertEqual(self.c, '')

    def test_pre_1_b(self):
        """test 1.2pre1b version string"""
        self.c = get_prerelease_suffix('1.2pre1b')
        self.assertEqual(self.c, '')

    def test_a_a(self):
        """test 1.2aa version string"""
        self.c = get_prerelease_suffix('1.2aa')
        self.assertEqual(self.c, '')

    def test_b_b(self):
        """test 1.2bb version string"""
        self.c = get_prerelease_suffix('1.2bb')
        self.assertEqual(self.c, '')

    def test_a_b(self):
        """test 1.2ab version string"""
        self.c = get_prerelease_suffix('1.2ab')
        self.assertEqual(self.c, '')

    def test_plus(self):
        """test 1.2+ version string """
        self.c = get_prerelease_suffix('1.2+')
        self.assertEqual(self.c, '')


if __name__ == '__main__':
    mozunit.main()
