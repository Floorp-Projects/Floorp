/*
Copyright (c) 2010 Serge A. Zaitsev
Copyright (c) 2015 Andreas Gal

Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <cctype>
#include "json.h"

using namespace std;

namespace JSON {

enum {
  PRIMITIVE = 0, // number, boolean, true, false, null
  OBJECT = 1,
  ARRAY = 2,
  STRING = 3
};

Token::Token(int _type, const string& _v) {
  type = _type;
  size = 0;
  v = _v;
}

bool Token::isPrimitive() const {
  return type == PRIMITIVE;
}

bool Token::isString() const {
  return type == STRING;
}

bool Token::isObject() const {
  return type == OBJECT;
}

bool Token::isArray() const {
  return type == ARRAY;
}

void Parser::addToken(int type, const string& value, stack<int>& parents) {
  tokens.push_back(Token(type, value));
  if (!parents.empty()) {
    // Add this token to the enclosing value.
    Token *t = &tokens[parents.top()];
    t->size++;
    // String keys only have one child value, so we pop the parent here.
    if (t->type == STRING)
      parents.pop();
  }
}

int Parser::parsePrimitive(const string& js, string::const_iterator& pos, stack<int>& parents) {
  string::const_iterator start = pos;

  for (; pos != js.cend(); pos++) {
    if (isspace(*pos) || *pos == ',' || *pos == ']' || *pos == '}') {
      addToken(PRIMITIVE, string(start, pos), parents);
      pos--;
      return OK;
    }
    if (*pos < 32 || *pos >= 127) {
      pos = start;
      return ERROR_INVAL;
    }
  }
  pos = start;
  return ERROR_PART;
}

int Parser::parseString(const string& js, string::const_iterator& pos, stack<int>& parents) {
  string::const_iterator start = pos;

  pos++; // skip starting quote

  for (; pos != js.cend(); pos++) {
    // Quote: end of string
    if (*pos == '\"') {
      addToken(STRING, string(start + 1, pos), parents);
      return OK;
    }

    // Backslash: Quoted symbol expected
    if (*pos == '\\' && pos + 1 < js.cend()) {
      pos++;
      switch (*pos) {
	// Allowed escaped symbols
      case '\"': case '/' : case '\\' : case 'b' :
      case 'f' : case 'r' : case 'n'  : case 't' :
	break;
	// Allows escaped symbol \uXXXX
      case 'u':
	pos++;
	for (int i = 0; i < 4 && pos < js.cend(); i++) {
	  if (!isxdigit(*pos)) {
	    pos = start;
	    return ERROR_INVAL;
	  }
	  pos++;
	}
	pos--;
	break;
	// Unexpected symbol
      default:
	pos = start;
	return ERROR_INVAL;
      }
    }
  }
  pos = start;
  return ERROR_PART;
}

int Parser::parse(const std::string& js) {
  stack<int> parents;
  string::const_iterator pos = js.cbegin();

  tokens.clear();

  for (; pos != js.cend(); pos++) {
    if (isspace(*pos))
      continue;
    switch (*pos) {
    case '{': case '[': {
      int type = (*pos == '{' ? OBJECT : ARRAY);
      addToken(type, string(), parents);
      parents.push(tokens.size() - 1);
      break;
    }
    case '}': case ']': {
      if (parents.empty())
	return ERROR_INVAL;
      int type = (*pos == '}' ? OBJECT : ARRAY);
      Token* t = &tokens[parents.top()];
      parents.pop();
      if (t->type != type)
	return ERROR_INVAL;
      break;
    }
    case '\"': {
      int r = parseString(js, pos, parents);
      if (r < 0) return r;
      break;
    }
    case ':': {
      if (tokens.empty() || tokens.back().type != STRING)
	return ERROR_INVAL;
      parents.push(tokens.size() - 1);
      break;
    }
    case ',': {
      if (parents.empty())
	return ERROR_INVAL;
      break;
    }
    case '-': case '0': case '1' : case '2': case '3' : case '4':
    case '5': case '6': case '7' : case '8': case '9':
    case 't': case 'f': case 'n' : {
      if (parents.empty())
	return ERROR_INVAL;
      Token *t = &tokens[parents.top()];
      if (t->type == OBJECT ||
	  (t->type == STRING && t->size != 0)) {
	return ERROR_INVAL;
      }
      int r = parsePrimitive(js, pos, parents);
      if (r < 0) return r;
      break;
    }
    default:
      // Unexpected character
      return ERROR_INVAL;
    }
  }

  if (!parents.empty())
    return ERROR_PART;

  return tokens.size();
}

}
