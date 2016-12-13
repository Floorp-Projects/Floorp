/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __rpc_h__
#define __rpc_h__

#include <cmath>
#include <cstring>
#include <iomanip>
#include <map>
#include <string>
#include <sstream>

#include "json/json.h"

extern void Fail(const char *reason, const char *data);

#ifdef INTERPOSE
const void* RealGetInterface(const char* interfaceName);
void Logging_PP_CompletionCallback(void* user_data, int32_t result);
void Logging_PPB_Audio_Callback_1_0(void* sample_buffer,
                                    uint32_t buffer_size_in_bytes,
                                    void* user_data);
struct Logging_PPB_Audio_Callback_1_0_holder {
  PPB_Audio_Callback_1_0 func;
  void* user_data;
};
struct Logging_PPP_Class_Deprecated_holder {
  const PPP_Class_Deprecated* _real_PPP_Class_Deprecated;
  const void* object;
};
extern const PPP_Class_Deprecated _interpose_PPP_Class_Deprecated_1_0;
#else // INTERPOSE
void ToHost(std::stringstream &s, bool abortIfNonMainThread);
std::string ToHostWithResult(std::stringstream &s, bool abortIfNonMainThread);

enum RPCType {
  WithResult,
  NoResult
};
enum RPCThread {
  MainThreadOnly,
  MaybeNonMainThread
};
template<RPCThread rpcThread>
void RPC(std::stringstream &s) {
  return ToHost(s, rpcThread == MainThreadOnly);
}
template<RPCThread rpcThread>
std::string RPCWithResult(std::stringstream &s) {
  return ToHostWithResult(s, rpcThread == MainThreadOnly);
}
#endif // INTERPOSE

typedef float float_t;
typedef double double_t;
typedef const void* mem_t;
typedef void* const * mem_ptr_t;
typedef const char* str_t;
typedef const char* cstr_t;
typedef int GLint;
typedef unsigned int GLuint;
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef unsigned char GLboolean;
typedef float GLclampf;
typedef float GLfloat;
typedef long int GLintptr;
typedef int GLsizei;
typedef long int GLsizeiptr;
typedef const GLint * GLint_ptr_t;
typedef const GLuint * GLuint_ptr_t;
typedef const GLenum * GLenum_ptr_t;
typedef const GLboolean * GLboolean_ptr_t;
typedef const GLfloat * GLfloat_ptr_t;
typedef const GLsizei * GLsizei_ptr_t;
typedef const GLubyte * GLubyte_ptr_t;

#ifdef _WIN32
#include<windows.h>
typedef HANDLE PP_FileHandle;
#else
typedef int PP_FileHandle;
#endif

struct PPP_MessageHandler_0_2;

static void BeginProps(std::stringstream &s) {
  s << '{';
}

static void EndProps(std::stringstream &s) {
  s << '}';
}

static void BeginProp(std::stringstream &s, const std::string &key) {
  if (s.str().back() != '{')
    s << ',';
  s << '"' << key << "\":";
}

static void AddProp(std::stringstream &s, const std::string &key, const std::string &value) {
  BeginProp(s, key);
  s << value;
}

static void BeginElements(std::stringstream &s) {
  s << '[';
}

static void EndElements(std::stringstream &s) {
  s << ']';
}

static void BeginElement(std::stringstream &s) {
  if (s.str().back() != '[')
    s << ',';
}

static void AddElement(std::stringstream &s, const std::string &value) {
  BeginElement(s);
  s << value;
}

// Define how to send certain primitive types to the browser.

template <typename T>
static const std::string ToString(const T *value) {
  std::stringstream x;
  x << *value;
  return x.str();
}

static void WriteEscapedChar(std::stringstream& x, const char value)
{
  switch (value) {
    case '\\':
      x << "\\\\";
      return;
    case '"':
      x << "\\" << value;
      return;
  }
  unsigned int v = (unsigned int)value;
  if (v <= 0x001f) {
    x << "\\u" << std::setw(4) << std::setfill('0') << std::hex << v;
  } else {
    x << value;
  }
}

template <typename T>
static void WriteString(std::stringstream& x, const T *value, size_t length) {
  const char* start = *value;
  const char* end = start + length;
  size_t pos = 0;
  while (start != end) {
    WriteEscapedChar(x, *(start++));
  }
}

template <typename T>
static const std::string StringToQuotedString(const T *value, size_t length) {
  std::stringstream x;
  x << '"';
  WriteString(x, value, length);
  x << '"';
  return x.str();
}

template <typename T>
static const std::string StringToQuotedString(const T *value) {
  return StringToQuotedString(value, strlen(*value));
}

static const std::string StringToQuotedString(const char *value) {
  return StringToQuotedString(&value, 1);
}

template <typename T>
static const std::string PointerToString(T *value) {
  std::stringstream x;
  if (!value) {
    x << "null";
  } else {
    x << (std::intptr_t)value;
  }
  return x.str();
}

#define DEFINE_TOSTRING_FORWARD_REF_TO_POINTER(T) \
  static const std::string ToString_##T(const T &value) { \
    return ToString_##T(&value); \
  }
#define DEFINE_TOSTRING(T) \
  static const std::string ToString_##T(const T *value) { \
    return ToString(value); \
  } \
  DEFINE_TOSTRING_FORWARD_REF_TO_POINTER(T)
#define DEFINE_INT_TOSTRING(T) \
  static const std::string ToString_##T(const T *value) { \
    std::stringstream x; \
    x << std::dec << (int)*value; \
    return x.str(); \
  } \
  DEFINE_TOSTRING_FORWARD_REF_TO_POINTER(T)
#define DEFINE_UNSIGNED_INT_TOSTRING(T) \
  static const std::string ToString_##T(const T *value) { \
    std::stringstream x; \
    x << std::dec << (unsigned int)*value; \
    return x.str(); \
  } \
  DEFINE_TOSTRING_FORWARD_REF_TO_POINTER(T)
#define DEFINE_FLOAT_TOSTRING(T) \
  static const std::string ToString_##T(const T *value) { \
    switch (std::fpclassify(*value)) { \
      case FP_INFINITE: \
      case FP_NAN: \
        return "null"; \
      default: \
        return ToString(value); \
    } \
  } \
  DEFINE_TOSTRING_FORWARD_REF_TO_POINTER(T)
#define DEFINE_POINTER_TOSTRING(T) \
  static const std::string ToString_##T(const T *value) { \
    return PointerToString(*value); \
  } \
  DEFINE_TOSTRING_FORWARD_REF_TO_POINTER(T)
#define DEFINE_STRING_TOSTRING(T) \
  static const std::string ToString_##T(const T *value) { \
    return StringToQuotedString(value); \
  } \
  DEFINE_TOSTRING_FORWARD_REF_TO_POINTER(T)

DEFINE_STRING_TOSTRING(char)
DEFINE_INT_TOSTRING(int8_t)
DEFINE_INT_TOSTRING(int32_t)
DEFINE_INT_TOSTRING(int64_t)
DEFINE_UNSIGNED_INT_TOSTRING(uint8_t)
DEFINE_UNSIGNED_INT_TOSTRING(uint16_t)
DEFINE_UNSIGNED_INT_TOSTRING(uint32_t)
DEFINE_UNSIGNED_INT_TOSTRING(uint64_t)
DEFINE_FLOAT_TOSTRING(float_t)
DEFINE_FLOAT_TOSTRING(double_t)
DEFINE_POINTER_TOSTRING(mem_t)
DEFINE_TOSTRING(mem_ptr_t)
DEFINE_STRING_TOSTRING(str_t)
static const std::string ToString_str_t(const str_t *value, size_t length) {
  return StringToQuotedString(value, length);
}
static const std::string ToString_str_t(const str_t &value, size_t length) {
  return StringToQuotedString(&value, length);
}
DEFINE_STRING_TOSTRING(cstr_t)
DEFINE_TOSTRING(GLint)
DEFINE_TOSTRING(GLuint)
DEFINE_TOSTRING(GLenum)
DEFINE_TOSTRING(GLbitfield)
static const std::string ToString_GLboolean(const GLboolean *value) {
  std::stringstream x;
  x << (*value ? "true" : "false");
  return x.str();
}
DEFINE_TOSTRING_FORWARD_REF_TO_POINTER(GLboolean)
DEFINE_TOSTRING(GLfloat)
DEFINE_TOSTRING(GLclampf)
DEFINE_TOSTRING(GLintptr)
DEFINE_TOSTRING(GLsizei)
DEFINE_TOSTRING(GLsizeiptr)
DEFINE_TOSTRING(GLuint_ptr_t)
DEFINE_TOSTRING(GLenum_ptr_t)
DEFINE_TOSTRING(GLboolean_ptr_t)
DEFINE_POINTER_TOSTRING(GLint_ptr_t)
DEFINE_POINTER_TOSTRING(GLfloat_ptr_t)
DEFINE_TOSTRING(GLsizei_ptr_t)
DEFINE_TOSTRING(PP_FileHandle)

#ifdef INTERPOSE
static const std::string ToString_GLubyte_ptr_t(const GLubyte *value) {
  std::stringstream x;
  x << (const char*)value;
  return x.str();
}
static const std::string ToString_uint16_ptr_t(const uint16_ptr_t value) {
  return "";
}
extern const std::string ToString_PP_DirContents_Dev(const PP_DirContents_Dev *v);
static const std::string ToString_PP_DirContents_Dev(PP_DirContents_Dev **value) {
  if (!value && !*value) {
    return "{}";
  }
  return ToString_PP_DirContents_Dev(*value);
}
#endif

// Define how to emit client objects that the client sends to the host. We just want
// to send a pointer value here, not actually unpack the struct.

static const std::string ToString_PPP_MessageHandler(const PPP_MessageHandler_0_2 *value) {
  std::stringstream x;
  x << value;
  return x.str();
}

static const std::string ToString_PPP_Class_Deprecated(const PPP_Class_Deprecated *value) {
  return PointerToString(value);
}


class JSONIterator {
  JSON::Parser parser;
  JSON::Parser::iterator iterator;
  JSON::Parser::iterator end;

public:
  JSONIterator(const std::string& json)
  {
    if (parser.parse(json) <= 0) {
      Fail("Fatal: failed to parse '%s'\n", json.c_str());
    }
    iterator = parser.begin();
    end = parser.end();
  }

  bool isValid() const {
    return iterator != end;
  }
  operator const JSON::Token&() const {
    return *iterator;
  }

  void skip() {
    ++iterator;
  }
  const JSON::Token& getCurrentAndGotoNext() {
    const JSON::Token& token = *iterator;
    ++iterator;
    return token;
  }

  const JSON::Token& getCurrentPrimitiveAndGotoNext()
  {
    const JSON::Token& token = getCurrentAndGotoNext();
    if (!token.isPrimitive()) {
      Fail("Expected primitive", "");
    };
    return token;
  }
  const JSON::Token& getCurrentStringAndGotoNext()
  {
    const JSON::Token& token = getCurrentAndGotoNext();
    if (!token.isString()) {
      Fail("Expected string", "");
    };
    return token;
  }

  const void expectObjectAndGotoFirstProperty()
  {
    if (!getCurrentAndGotoNext().isObject()) {
      Fail("Expected object", "");
    }
    if (!iterator->isString()) {
      Fail("Expected string", "");
    }
  }
  const size_t expectArrayAndGotoFirstItem()
  {
    const JSON::Token& token = getCurrentAndGotoNext();
    if (!token.isArray()) {
      Fail("Expected array", "");
    }
    return token.children();
  }
};

template <typename T>
struct OutParam
{
  typedef typename std::conditional<std::is_pointer<T>::value, typename std::remove_pointer<T>::type, T>::type nopointer;
  typedef typename std::remove_const<nopointer>::type noconst;
  typedef typename std::conditional<std::is_pointer<T>::value, typename std::add_pointer<noconst>::type, noconst>::type type;
};

static void FromJSON_int(JSONIterator& iterator, long int& value)
{
  value = atol(iterator.getCurrentPrimitiveAndGotoNext().value().c_str());
}

static void FromJSON_uintptr(JSONIterator& iterator, std::uintptr_t& value)
{
  long long pointer = std::atoll(iterator.getCurrentPrimitiveAndGotoNext().value().c_str());
  value = static_cast<std::uintptr_t>(pointer);
}

template <typename T>
static void PointerValueFromJSON(JSONIterator& iterator, T*& value) {
  std::uintptr_t pointer;
  FromJSON_uintptr(iterator, pointer);
  value = (T*)pointer;
}

static void FromJSON_charArray(JSONIterator& iterator, char* value, size_t count)
{
  const JSON::Token& token = iterator.getCurrentStringAndGotoNext();
  std::strncpy(value, token.value().c_str(), count);
}

// FIXME Check range?
#define DEFINE_FROMJSON_INT(T) \
  static void FromJSON_##T(JSONIterator& iterator, OutParam<T>::type& value) { \
    long int v; \
    FromJSON_int(iterator, v); \
    value = v; \
  }

#define DEFINE_FROMJSON_FLOAT(T) \
static void FromJSON_##T(JSONIterator& iterator, OutParam<T>::type& value) \
  { \
    value = atof(iterator.getCurrentPrimitiveAndGotoNext().value().c_str()); \
  }

DEFINE_FROMJSON_INT(int8_t)
DEFINE_FROMJSON_INT(int32_t)
DEFINE_FROMJSON_INT(int64_t)
DEFINE_FROMJSON_INT(uint8_t)
DEFINE_FROMJSON_INT(uint16_t)
DEFINE_FROMJSON_INT(uint32_t)
DEFINE_FROMJSON_INT(uint64_t)
DEFINE_FROMJSON_FLOAT(float_t)
DEFINE_FROMJSON_FLOAT(double_t)
static void FromJSON_mem_t(JSONIterator& iterator, OutParam<mem_t>::type& value)
{
  PointerValueFromJSON(iterator, value);
}
static void FromJSON_mem_ptr_t(JSONIterator& iterator, OutParam<mem_ptr_t>::type& value)
{
  const JSON::Token& token = iterator.getCurrentAndGotoNext();
  Fail("UNIMPLEMENTED: %s\n", "FromJSON_mem_ptr_t");
}
static void FromJSON_str_t(JSONIterator& iterator, OutParam<str_t>::type& value)
{
  const JSON::Token& token = iterator.getCurrentAndGotoNext();
  if (token.isString()) {
    size_t length = 0;
    std::string tokenValue = token.value();
    for (std::string::iterator it = tokenValue.begin(); it != tokenValue.cend(); ++it) {
      ++length;
      if (*it == '\\' && it + 1 != tokenValue.cend()) {
        std::string::iterator next = it + 1;
        switch (*next) {
          case '\"':
          case '/':
          case '\\':
            tokenValue.erase(it);
            break;
          case 'b':
            tokenValue.replace(it, next + 1, "\b");
            break;
          case 'f':
            tokenValue.replace(it, next + 1, "\f");
            break;
          case 'r':
            tokenValue.replace(it, next + 1, "\r");
            break;
          case 'n':
            tokenValue.replace(it, next + 1, "\n");
            break;
          case 't':
            tokenValue.replace(it, next + 1, "\t");
            break;
          case 'u':
            if (tokenValue.cend() - next >= 5) {
              if (*(next + 1) == '0' &&
                  *(next + 2) == '0' &&
                  *(next + 3) == '0' &&
                  *(next + 4) == '0') {
                tokenValue.replace(it, next + 5, 1, '\0');
                break;
              }
              Fail("Need to handle unicode escapes in strings: %s.",
                   tokenValue.c_str());
            }
        }
      }
    }
    value = (char*) malloc(length + 1);
    std::memcpy(value, tokenValue.c_str(), length + 1);
    return;
  }

  if (!token.isArray()) {
    Fail("Expected array", "");
    return;
  }

  size_t size = token.children();
  char* buff = new char[size];
  for (size_t i = 0; i < size; ++i) {
    buff[i] = iterator.getCurrentAndGotoNext().value()[0];
  }
  value = buff;
}
static void FromJSON_str_t(JSONIterator& iterator, str_t& value)
{
  return FromJSON_str_t(iterator, const_cast<OutParam<str_t>::type&>(value));
}
static void FromJSON_cstr_t(JSONIterator& iterator, cstr_t& value)
{
  value = strdup(iterator.getCurrentStringAndGotoNext().value().c_str());
}
DEFINE_FROMJSON_INT(GLint)
DEFINE_FROMJSON_INT(GLuint)
static void FromJSON_GLenum(JSONIterator& iterator, OutParam<GLenum>::type& value)
{
  const JSON::Token& token = iterator.getCurrentAndGotoNext();
  Fail("UNIMPLEMENTED: %s\n", "FromJSON_GLenum");
}
static void FromJSON_GLbitfield(JSONIterator& iterator, OutParam<GLbitfield>::type& value)
{
  const JSON::Token& token = iterator.getCurrentAndGotoNext();
  Fail("UNIMPLEMENTED: %s\n", "FromJSON_GLbitfield");
}
DEFINE_FROMJSON_INT(GLboolean)
static void FromJSON_GLfloat(JSONIterator& iterator, OutParam<GLfloat>::type& value)
{
  const JSON::Token& token = iterator.getCurrentAndGotoNext();
  Fail("UNIMPLEMENTED: %s\n", "FromJSON_GLfloat");
}
static void FromJSON_GLclampf(JSONIterator& iterator, OutParam<GLclampf>::type& value)
{
  const JSON::Token& token = iterator.getCurrentAndGotoNext();
  Fail("UNIMPLEMENTED: %s\n", "FromJSON_GLclampf");
}
static void FromJSON_GLintptr(JSONIterator& iterator, OutParam<GLintptr>::type& value)
{
  const JSON::Token& token = iterator.getCurrentAndGotoNext();
  Fail("UNIMPLEMENTED: %s\n", "FromJSON_GLintptr");
}
DEFINE_FROMJSON_INT(GLsizei)
static void FromJSON_GLboolean_ptr_t(JSONIterator& iterator, OutParam<GLboolean_ptr_t>::type& value)
{
  const JSON::Token& token = iterator.getCurrentAndGotoNext();
  Fail("UNIMPLEMENTED: %s\n", "FromJSON_GLboolean_ptr_t");
}
static void FromJSON_GLenum_ptr_t(JSONIterator& iterator, OutParam<GLenum_ptr_t>::type& value)
{
  const JSON::Token& token = iterator.getCurrentAndGotoNext();
  Fail("UNIMPLEMENTED: %s\n", "FromJSON_GLenum_ptr_t");
}
static void FromJSON_GLfloat_ptr_t(JSONIterator& iterator,OutParam<GLfloat_ptr_t>::type& value)
{
  const JSON::Token& token = iterator.getCurrentAndGotoNext();
  Fail("UNIMPLEMENTED: %s\n", "FromJSON_GLfloat_ptr_t");
}
static void FromJSON_GLint_ptr_t(JSONIterator& iterator, OutParam<GLint_ptr_t>::type& value)
{
  return FromJSON_GLint(iterator, *value);
  Fail("UNIMPLEMENTED: %s\n", "FromJSON_GLint_ptr_t");
}
static void FromJSON_GLsizei_ptr_t(JSONIterator& iterator, OutParam<GLsizei_ptr_t>::type& value)
{
  if (!value) {
    iterator.skip();
  } else {
    FromJSON_GLsizei(iterator, *value);
  }
}
static void FromJSON_GLubyte_ptr_t(JSONIterator& iterator, OutParam<GLubyte_ptr_t>::type& value)
{
  const JSON::Token& token = iterator.getCurrentStringAndGotoNext();
  std::string tokenValue = token.value();
  size_t size = tokenValue.size();
  value = new GLubyte[size];
  std::memcpy(value, tokenValue.data(), size);
}
static void FromJSON_GLuint_ptr_t(JSONIterator& iterator, OutParam<GLuint_ptr_t>::type& value)
{
  const JSON::Token& token = iterator.getCurrentAndGotoNext();
  Fail("UNIMPLEMENTED: %s\n", "FromJSON_GLuint_ptr_t");
}
#ifdef _WIN32
static void FromJSON_PP_FileHandle(JSONIterator& iterator, PP_FileHandle& value)
{
  PointerValueFromJSON(iterator, value);
}
#else
DEFINE_FROMJSON_INT(PP_FileHandle)
#endif
static void FromJSON_uint16_ptr_t(JSONIterator& iterator, uint16_ptr_t& value)
{
  PointerValueFromJSON(iterator, value);
}

struct PP_Flash_Menu;
void FromJSON_PP_Flash_Menu(JSONIterator& iterator, PP_Flash_Menu &value);
void FromJSON_PP_Flash_Menu(JSONIterator& iterator, PP_Flash_Menu *&value);

struct PP_DirEntry_Dev;
struct PP_DirContents_Dev;
void FromJSON_PP_DirEntry_Dev(JSONIterator& iterator, PP_DirEntry_Dev &value);
void FromJSON_PP_DirContents_Dev(JSONIterator& iterator, PP_DirContents_Dev *&value);

#endif
