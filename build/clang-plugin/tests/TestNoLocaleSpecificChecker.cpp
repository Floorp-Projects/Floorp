// Locale-specific standard library functions are banned.

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <string>

int useLocaleSpecificFunctions() {
  if (isalpha('r')) {  // expected-warning{{locale-specific}} expected-note {{IsAsciiAlpha instead}}
    return 1;
  }
  if (isalnum('8')) {  // expected-warning{{locale-specific}} expected-note {{IsAsciiAlphanumeric instead}}
    return 1;
  }
  std::string s("8167");
  if (std::find_if(s.begin(), s.end(), isdigit) != s.end()) {  // expected-warning{{locale-specific}} expected-note {{IsAsciiDigit instead}}
    return 1;
  }

  if (strcasecmp(s.c_str(), "81d7") == 0) { // expected-warning{{locale-specific}} expected-note {{CompareCaseInsensitiveASCII}}
      return 1;
  }

  printf("problems: %s\n", strerror(errno)); // expected-warning{{locale-specific}}

  return 0;
}
