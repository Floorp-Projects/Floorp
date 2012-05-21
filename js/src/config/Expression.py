# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Parses and evaluates simple statements for Preprocessor:

Expression currently supports the following grammar, whitespace is ignored:

expression :
  unary ( ( '==' | '!=' ) unary ) ? ;
unary :
  '!'? value ;
value :
  [0-9]+ # integer
  | \w+  # string identifier or value;
"""

import re

class Expression:
  def __init__(self, expression_string):
    """
    Create a new expression with this string.
    The expression will already be parsed into an Abstract Syntax Tree.
    """
    self.content = expression_string
    self.offset = 0
    self.__ignore_whitespace()
    self.e = self.__get_equality()
    if self.content:
      raise Expression.ParseError, self

  def __get_equality(self):
    """
    Production: unary ( ( '==' | '!=' ) unary ) ?
    """
    if not len(self.content):
      return None
    rv = Expression.__AST("equality")
    # unary 
    rv.append(self.__get_unary())
    self.__ignore_whitespace()
    if not re.match('[=!]=', self.content):
      # no equality needed, short cut to our prime unary
      return rv[0]
    # append operator
    rv.append(Expression.__ASTLeaf('op', self.content[:2]))
    self.__strip(2)
    self.__ignore_whitespace()
    rv.append(self.__get_unary())
    self.__ignore_whitespace()
    return rv

  def __get_unary(self):
    """
    Production: '!'? value
    """
    # eat whitespace right away, too
    not_ws = re.match('!\s*', self.content)
    if not not_ws:
      return self.__get_value()
    rv = Expression.__AST('not')
    self.__strip(not_ws.end())
    rv.append(self.__get_value())
    self.__ignore_whitespace()
    return rv

  def __get_value(self):
    """
    Production: ( [0-9]+ | \w+)
    Note that the order is important, and the expression is kind-of
    ambiguous as \w includes 0-9. One could make it unambiguous by
    removing 0-9 from the first char of a string literal.
    """
    rv = None
    word_len = re.match('[0-9]*', self.content).end()
    if word_len:
      value = int(self.content[:word_len])
      rv = Expression.__ASTLeaf('int', value)
    else:
      word_len = re.match('\w*', self.content).end()
      if word_len:
        rv = Expression.__ASTLeaf('string', self.content[:word_len])
      else:
        raise Expression.ParseError, self
    self.__strip(word_len)
    self.__ignore_whitespace()
    return rv

  def __ignore_whitespace(self):
    ws_len = re.match('\s*', self.content).end()
    self.__strip(ws_len)
    return

  def __strip(self, length):
    """
    Remove a given amount of chars from the input and update
    the offset.
    """
    self.content = self.content[length:]
    self.offset += length
  
  def evaluate(self, context):
    """
    Evaluate the expression with the given context
    """
    
    # Helper function to evaluate __get_equality results
    def eval_equality(tok):
      left = opmap[tok[0].type](tok[0])
      right = opmap[tok[2].type](tok[2])
      rv = left == right
      if tok[1].value == '!=':
        rv = not rv
      return rv
    # Mapping from token types to evaluator functions
    # Apart from (non-)equality, all these can be simple lambda forms.
    opmap = {
      'equality': eval_equality,
      'not': lambda tok: not opmap[tok[0].type](tok[0]),
      'string': lambda tok: context[tok.value],
      'int': lambda tok: tok.value}

    return opmap[self.e.type](self.e);
  
  class __AST(list):
    """
    Internal class implementing Abstract Syntax Tree nodes
    """
    def __init__(self, type):
      self.type = type
      super(self.__class__, self).__init__(self)
  
  class __ASTLeaf:
    """
    Internal class implementing Abstract Syntax Tree leafs
    """
    def __init__(self, type, value):
      self.value = value
      self.type = type
    def __str__(self):
      return self.value.__str__()
    def __repr__(self):
      return self.value.__repr__()
  
  class ParseError(StandardError):
    """
    Error raised when parsing fails.
    It has two members, offset and content, which give the offset of the
    error and the offending content.
    """
    def __init__(self, expression):
      self.offset = expression.offset
      self.content = expression.content[:3]
    def __str__(self):
      return 'Unexpected content at offset %i, "%s"'%(self.offset, self.content)

class Context(dict):
  """
  This class holds variable values by subclassing dict, and while it
  truthfully reports True and False on
  
  name in context
  
  it returns the variable name itself on
  
  context["name"]

  to reflect the ambiguity between string literals and preprocessor
  variables.
  """
  def __getitem__(self, key):
    if key in self:
      return super(self.__class__, self).__getitem__(key)
    return key
