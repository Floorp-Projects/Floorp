import unittest

import pymake.data
import pymake.functions

class VariableRefTest(unittest.TestCase):
    def test_get_expansions(self):
        e = pymake.data.StringExpansion('FOO', None)
        f = pymake.functions.VariableRef(None, e)

        exps = list(f.expansions())
        self.assertEqual(len(exps), 1)

class GetExpansionsTest(unittest.TestCase):
    def test_get_arguments(self):
        f = pymake.functions.SubstFunction(None)

        e1 = pymake.data.StringExpansion('FOO', None)
        e2 = pymake.data.StringExpansion('BAR', None)
        e3 = pymake.data.StringExpansion('BAZ', None)

        f.append(e1)
        f.append(e2)
        f.append(e3)

        exps = list(f.expansions())
        self.assertEqual(len(exps), 3)

    def test_descend(self):
        f = pymake.functions.StripFunction(None)

        e = pymake.data.Expansion(None)

        e1 = pymake.data.StringExpansion('FOO', None)
        f1 = pymake.functions.VariableRef(None, e1)
        e.appendfunc(f1)

        f2 = pymake.functions.WildcardFunction(None)
        e2 = pymake.data.StringExpansion('foo/*', None)
        f2.append(e2)
        e.appendfunc(f2)

        f.append(e)

        exps = list(f.expansions())
        self.assertEqual(len(exps), 1)

        exps = list(f.expansions(True))
        self.assertEqual(len(exps), 3)

        self.assertFalse(f.is_filesystem_dependent)

if __name__ == '__main__':
    unittest.main()
