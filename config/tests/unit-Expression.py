import unittest

import sys
import os.path
import mozunit

from Expression import Expression, Context

class TestContext(unittest.TestCase):
  """
  Unit tests for the Context class
  """

  def setUp(self):
    self.c = Context()
    self.c['FAIL'] = 'PASS'

  def test_string_literal(self):
    """test string literal, fall-through for undefined var in a Context"""
    self.assertEqual(self.c['PASS'], 'PASS')

  def test_variable(self):
    """test value for defined var in the Context class"""
    self.assertEqual(self.c['FAIL'], 'PASS')

  def test_in(self):
    """test 'var in context' to not fall for fallback"""
    self.assert_('FAIL' in self.c)
    self.assert_('PASS' not in self.c)

class TestExpression(unittest.TestCase):
  """
  Unit tests for the Expression class
  evaluate() is called with a context {FAIL: 'PASS'}
  """

  def setUp(self):
    self.c = Context()
    self.c['FAIL'] = 'PASS'

  def test_string_literal(self):
    """Test for a string literal in an Expression"""
    self.assertEqual(Expression('PASS').evaluate(self.c), 'PASS')

  def test_variable(self):
    """Test for variable value in an Expression"""
    self.assertEqual(Expression('FAIL').evaluate(self.c), 'PASS')

  def test_not(self):
    """Test for the ! operator"""
    self.assert_(Expression('!0').evaluate(self.c))
    self.assert_(not Expression('!1').evaluate(self.c))

  def test_equals(self):
    """ Test for the == operator"""
    self.assert_(Expression('FAIL == PASS').evaluate(self.c))

  def test_notequals(self):
    """ Test for the != operator"""
    self.assert_(Expression('FAIL != 1').evaluate(self.c))

if __name__ == '__main__':
  mozunit.main()
