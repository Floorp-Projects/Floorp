#ifndef __JSON_H_
#define __JSON_H_

#include <stddef.h>
#include <string>
#include <vector>
#include <stack>

namespace JSON {

enum {
  OK = 0,
  ERROR_INVAL = -1, /* Invalid character inside JSON string */
  ERROR_PART = -2 /* The string is not a full JSON packet, more bytes expected */
};

class Token {
  friend class Parser;

protected:
  int type;
  int size;
  std::string v;

public:
  Token(int _type, const std::string& _v);

  bool isPrimitive() const;
  bool isString() const;
  bool isObject() const;
  bool isArray() const;

  std::string value() const {
    return v;
  }

  size_t children() const {
    return size;
  }
};

class Parser {
  std::vector<Token> tokens;

  void addToken(int type, const std::string& value, std::stack<int>& parents);
  int parsePrimitive(const std::string& js, std::string::const_iterator& pos, std::stack<int>& parents);
  int parseString(const std::string& js, std::string::const_iterator& pos, std::stack<int>& parents);
public:
  int parse(const std::string& js);

  typedef std::vector<Token>::const_iterator iterator;

  iterator begin() const { return tokens.cbegin(); }
  iterator end() const { return tokens.cend(); }

  const Token& operator[](int index) const {
    return tokens[index];
  }

  size_t size() const {
    return tokens.size();
  }
};

}

#endif /* __JSON_H_ */
