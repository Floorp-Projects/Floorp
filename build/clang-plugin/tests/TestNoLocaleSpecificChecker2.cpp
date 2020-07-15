// Like TestNoLocaleSpecificChecker.cpp, but using the standard C++ <cctype> and <locale> headers.

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <locale>
#include <string>

int useLocaleSpecificFunctions() {
  if (isalpha('r')) {  // expected-warning{{locale-specific}} expected-note {{IsAsciiAlpha instead}}
    return 1;
  }
  using std::isalnum;
  if (isalnum('8')) {  // expected-warning{{locale-specific}} expected-note {{IsAsciiAlphanumeric instead}}
    return 1;
  }
  std::string s("8167");
  if (std::find_if(s.begin(), s.end(), isdigit) != s.end()) {  // expected-warning{{locale-specific}} expected-note {{IsAsciiDigit instead}}
    return 1;
  }
  typedef int (*PredicateType)(int);
  PredicateType predicate = std::isdigit;  // expected-warning{{locale-specific}} expected-note {{IsAsciiDigit instead}}

  if (strcasecmp(s.c_str(), "81d7") == 0) { // expected-warning{{locale-specific}} expected-note {{CompareCaseInsensitiveASCII}}
      return 1;
  }

  printf("problems: %s\n", strerror(errno)); // expected-warning{{locale-specific}}

  return 0;
}

using std::isalnum;

int useStdLocaleFunctions() {
  std::locale l;

  if (std::isalpha('r', l)) {  // expected-warning{{locale-specific}} expected-note {{IsAsciiAlpha instead}}
    return 1;
  }

  if (isalnum('8', l)) {  // expected-warning{{locale-specific}} expected-note {{IsAsciiAlphanumeric instead}}
    return 1;
  }

  using std::isdigit;
  std::string s("8167");
  if (std::find_if(s.begin(), s.end(), [&](char c) { return isdigit(c, l); }) != s.end()) {  // expected-warning{{locale-specific}} expected-note {{IsAsciiDigit instead}}
    return 1;
  }

  return 0;
}
