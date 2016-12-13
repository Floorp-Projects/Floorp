#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int test_passed = 0;
static int test_failed = 0;

/* Terminate current test with error */
#define fail()	return __LINE__

/* Successfull end of the test case */
#define done() return 0

/* Check single condition */
#define check(cond) do { if (!(cond)) fail(); } while (0)

/* Test runner */
static void test(int (*func)(void), const char *name) {
  int r = func();
  if (r == 0) {
    test_passed++;
  } else {
    test_failed++;
    printf("FAILED: %s (at line %d)\n", name, r);
  }
}

#include "json.cpp"

using namespace std;
using namespace JSON;

static bool TOKEN_STRING(const Token& t, const char *s) {
  return !t.value().compare(string(s));
}

static bool isObject(const Token& t, int children) {
  return t.isObject() && t.children() == children;
}

static bool isArray(const Token& t, int children) {
  return t.isArray() && t.children() == children;
}

static bool isKey(const Token& t, const char* v) {
  return t.isString() && t.children() == 1 && t.value() == v;
}

static bool isPrimitive(const Token& t, const char* v) {
  return t.isPrimitive() && t.children() == 0 && t.value() == v;
}

int test_empty() {
  const char *js;
  int r;
  Parser p;

  js = "{}";
  r = p.parse(string(js));
  check(r == 1);
  check(isObject(p[0], 0));

  js = "[]";
  r = p.parse(string(js));
  check(r == 1);
  check(isArray(p[0], 0));

  js = "{\"a\":[]}";
  r = p.parse(string(js));
  check(r == 3);
  check(isObject(p[0], 1));
  check(isKey(p[1], "a"));
  check(isArray(p[2], 0));

  js = "[{},{}]";
  r = p.parse(string(js));
  check(r >= 0);
  check(isArray(p[0], 2));
  check(isObject(p[1], 0));
  check(isObject(p[2], 0));
  return 0;
}

int test_simple() {
  const char *js;
  int r;
  Parser p;

  js = "{\"a\": 0}";

  r = p.parse(string(js));
  check(r == 3);
  check(isObject(p[0], 1));
  check(isKey(p[1], "a"));
  check(isPrimitive(p[2], "0"));

  js = "[\"a\":{},\"b\":{}]";
  r = p.parse(string(js));
  check(r == 5);
  check(isArray(p[0], 2));
  check(isKey(p[1], "a"));
  check(isObject(p[2], 0));
  check(isKey(p[3], "b"));
  check(isObject(p[4], 0));

  js = "{\n \"Day\": 26,\n \"Month\": 9,\n \"Year\": 12\n }";
  r = p.parse(string(js));
  check(r == 7);

  return 0;
}

int test_primitive() {
  int r;
  Parser p;
  const char *js;
  js = "{\"boolVar\" : true}";
  r = p.parse(string(js));
  check(r == 3);
  check(p[0].isObject() && p[1].isString() && p[2].isPrimitive());
  check(TOKEN_STRING(p[1], "boolVar"));
  check(TOKEN_STRING(p[2], "true"));

  js = "{\"boolVar\" : false}";
  r = p.parse(string(js));
  check(r == 3);
  check(p[0].isObject() && p[1].isString() && p[2].isPrimitive());
  check(TOKEN_STRING(p[1], "boolVar"));
  check(TOKEN_STRING(p[2], "false"));

  js = "{\"intVar\" : 12345}";
  r = p.parse(string(js));
  check(r == 3);
  check(p[0].isObject() && p[1].isString() && p[2].isPrimitive());
  check(TOKEN_STRING(p[1], "intVar"));
  check(TOKEN_STRING(p[2], "12345"));

  js = "{\"floatVar\" : 12.345}";
  r = p.parse(string(js));
  check(r == 3);
  check(p[0].isObject() && p[1].isString() && p[2].isPrimitive());
  check(TOKEN_STRING(p[1], "floatVar"));
  check(TOKEN_STRING(p[2], "12.345"));

  js = "{\"nullVar\" : null}";
  r = p.parse(string(js));
  check(r == 3);
  check(p[0].isObject() && p[1].isString() && p[2].isPrimitive());
  check(TOKEN_STRING(p[1], "nullVar"));
  check(TOKEN_STRING(p[2], "null"));

  return 0;
}

int test_string() {
  int r;
  Parser p;
  const char *js;

  js = "\"strVar\" : \"hello world\"";
  r = p.parse(string(js));
  check(r >= 0 && p[0].isString()
	&& p[1].isString());
  check(TOKEN_STRING(p[0], "strVar"));
  check(TOKEN_STRING(p[1], "hello world"));

  js = "\"strVar\" : \"escapes: \\/\\r\\n\\t\\b\\f\\\"\\\\\"";
  r = p.parse(string(js));
  check(r >= 0 && p[0].isString()
	&& p[1].isString());
  check(TOKEN_STRING(p[0], "strVar"));
  check(TOKEN_STRING(p[1], "escapes: \\/\\r\\n\\t\\b\\f\\\"\\\\"));

  js = "\"strVar\" : \"\"";
  r = p.parse(string(js));
  check(r >= 0 && p[0].isString()
	&& p[1].isString());
  check(TOKEN_STRING(p[0], "strVar"));
  check(TOKEN_STRING(p[1], ""));

  return 0;
}

int test_partial_string() {
  int r;
  Parser p;
  const char *js;

  js = "\"x\": \"va";
  r = p.parse(string(js));
  check(r == ERROR_PART && p[0].isString());
  check(TOKEN_STRING(p[0], "x"));
  check(p.size() == 1);

  char js_slash[] = "\"x\": \"va\\";
  r = p.parse(string(js_slash)); // intentionally not sizeof(js_slash)
  check(r == ERROR_PART);

  char js_unicode[] = "\"x\": \"va\\u";
  r = p.parse(string(js_unicode)); // intentionally not sizeof(js_unicode)
  check(r == ERROR_PART);

  js = "\"x\": \"valu";
  r = p.parse(string(js));
  check(r == ERROR_PART && p[0].isString());
  check(TOKEN_STRING(p[0], "x"));
  check(p.size() == 1);

  js = "\"x\": \"value\"";
  r = p.parse(string(js));
  check(r >= 0 && p[0].isString()
	&& p[1].isString());
  check(TOKEN_STRING(p[0], "x"));
  check(TOKEN_STRING(p[1], "value"));

  js = "{\"x\": \"value\", \"y\": \"value y\"}";
  r = p.parse(string(js));
  check(r >= 0
	&& p[0].isObject()
	&& p[1].isString()
	&& p[2].isString()
	&& p[3].isString()
	&& p[4].isString());
  check(TOKEN_STRING(p[1], "x"));
  check(TOKEN_STRING(p[2], "value"));
  check(TOKEN_STRING(p[3], "y"));
  check(TOKEN_STRING(p[4], "value y"));

  return 0;
}

int test_partial_array() {
  int r;
  Parser p;
  const char *js;

  js = "  [ 1, true, ";
  r = p.parse(string(js));
  check(r == ERROR_PART && p[0].isArray()
	&& p[1].isPrimitive() && p[2].isPrimitive());

  js = "  [ 1, true, [123, \"hello";
  r = p.parse(string(js));
  check(r == ERROR_PART && p[0].isArray()
	&& p[1].isPrimitive() && p[2].isPrimitive()
	&& p[3].isArray() && p[4].isPrimitive());

  js = "  [ 1, true, [123, \"hello\"]";
  r = p.parse(string(js));
  check(r == ERROR_PART && p[0].isArray()
	&& p[1].isPrimitive() && p[2].isPrimitive()
	&& p[3].isArray() && p[4].isPrimitive()
	&& p[5].isString());
  /* check child nodes of the 2nd array */
  check(p[3].children() == 2);

  js = "  [ 1, true, [123, \"hello\"]]";
  r = p.parse(string(js));
  check(r >= 0 && p[0].isArray()
	&& p[1].isPrimitive() && p[2].isPrimitive()
	&& p[3].isArray() && p[4].isPrimitive()
	&& p[5].isString());
  check(p[3].children() == 2);
  check(p[0].children() == 3);
  return 0;
}

int test_objects_arrays() {
  int r;
  Parser p;
  const char *js;

  js = "[10}";
  r = p.parse(string(js));
  check(r == ERROR_INVAL);

  js = "[10]";
  r = p.parse(string(js));
  check(r >= 0);

  js = "{\"a\": 1]";
  r = p.parse(string(js));
  check(r == ERROR_INVAL);

  js = "{\"a\": 1}";
  r = p.parse(string(js));
  check(r >= 0);

  return 0;
}

int test_issue_22() {
  int r;
  Parser p;
  const char *js;

  js = "{ \"height\":10, \"layers\":[ { \"data\":[6,6], \"height\":10, "
    "\"name\":\"Calque de Tile 1\", \"opacity\":1, \"type\":\"tilelayer\", "
    "\"visible\":true, \"width\":10, \"x\":0, \"y\":0 }], "
    "\"orientation\":\"orthogonal\", \"properties\": { }, \"tileheight\":32, "
    "\"tilesets\":[ { \"firstgid\":1, \"image\":\"..\\/images\\/tiles.png\", "
    "\"imageheight\":64, \"imagewidth\":160, \"margin\":0, \"name\":\"Tiles\", "
    "\"properties\":{}, \"spacing\":0, \"tileheight\":32, \"tilewidth\":32 }], "
    "\"tilewidth\":32, \"version\":1, \"width\":10 }";
  r = p.parse(string(js));
  check(r >= 0);
  return 0;
}

int test_unicode_characters() {
  Parser p;
  const char *js;

  int r;
  js = "{\"a\":\"\\uAbcD\"}";
  r = p.parse(string(js));
  check(r >= 0);

  js = "{\"a\":\"str\\u0000\"}";
  r = p.parse(string(js));
  check(r >= 0);

  js = "{\"a\":\"\\uFFFFstr\"}";
  r = p.parse(string(js));
  check(r >= 0);

  js = "{\"a\":\"str\\uFFGFstr\"}";
  r = p.parse(string(js));
  check(r == ERROR_INVAL);

  js = "{\"a\":\"str\\u@FfF\"}";
  r = p.parse(string(js));
  check(r == ERROR_INVAL);

  js = "{\"a\":[\"\\u028\"]}";
  r = p.parse(string(js));
  check(r == ERROR_INVAL);

  js = "{\"a\":[\"\\u0280\"]}";
  r = p.parse(string(js));
  check(r >= 0);

  return 0;
}

int test_count() {
  Parser p;
  const char *js;

  js = "{}";
  check(p.parse(string(js)) == 1);

  js = "[]";
  check(p.parse(string(js)) == 1);

  js = "[[]]";
  check(p.parse(string(js)) == 2);

  js = "[[], []]";
  check(p.parse(string(js)) == 3);

  js = "[[], []]";
  check(p.parse(string(js)) == 3);

  js = "[[], [[]], [[], []]]";
  check(p.parse(string(js)) == 7);

  js = "[\"a\", [[], []]]";
  check(p.parse(string(js)) == 5);

  js = "[[], \"[], [[]]\", [[]]]";
  check(p.parse(string(js)) == 5);

  js = "[1, 2, 3]";
  check(p.parse(string(js)) == 4);

  js = "[1, 2, [3, \"a\"], null]";
  check(p.parse(string(js)) == 7);

  return 0;
}

int test_keyvalue() {
  const char *js;
  int r;
  Parser p;
  vector<Token> tokens;

  js = "{\"a\": 0, \"b\": \"c\"}";

  r = p.parse(string(js));
  check(r == 5);
  check(p[0].children() == 2); /* two keys */
  check(p[1].children() == 1 && p[3].children() == 1); /* one value per key */
  check(p[2].children() == 0 && p[4].children() == 0); /* values have zero size */

  js = "{\"a\"\n0}";
  r = p.parse(string(js));
  check(r == ERROR_INVAL);

  js = "{\"a\", 0}";
  r = p.parse(string(js));
  check(r == ERROR_INVAL);

  js = "{\"a\": {2}}";
  r = p.parse(string(js));
  check(r == ERROR_INVAL);

  js = "{\"a\": {2: 3}}";
  r = p.parse(string(js));
  check(r == ERROR_INVAL);

  js = "{\"a\": {\"a\": 2 3}}";
  r = p.parse(string(js));
  check(r == ERROR_INVAL);
  return 0;
}

int main() {
  test(test_empty, "general test for a empty JSON objects/arrays");
  test(test_simple, "general test for a simple JSON string");
  test(test_primitive, "test primitive JSON data types");
  test(test_string, "test string JSON data types");
  test(test_partial_string, "test partial JSON string parsing");
  test(test_partial_array, "test partial array reading");
  test(test_objects_arrays, "test objects and arrays");
  test(test_unicode_characters, "test unicode characters");
  test(test_issue_22, "test issue #22");
  test(test_count, "test tokens count estimation");
  test(test_keyvalue, "test for keys/values");
  printf("PASSED: %d\nFAILED: %d\n", test_passed, test_failed);
  return 0;
}
