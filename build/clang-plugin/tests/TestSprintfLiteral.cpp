#include <cstdio>

void bad() {
  char x[100];
  snprintf(x, sizeof(x), "bar"); // expected-error {{Use SprintfLiteral instead of snprintf when writing into a character array.}} expected-note {{This will prevent passing in the wrong size to snprintf accidentally.}}
  snprintf(x, 100, "bar"); // expected-error {{Use SprintfLiteral instead of snprintf when writing into a character array.}} expected-note {{This will prevent passing in the wrong size to snprintf accidentally.}}
  const int hundred = 100;
  snprintf(x, hundred, "bar"); // expected-error {{Use SprintfLiteral instead of snprintf when writing into a character array.}} expected-note {{This will prevent passing in the wrong size to snprintf accidentally.}}
}

void ok() {
  char x[100];
  int y;
  snprintf(x, sizeof(y), "foo");

  snprintf(x, 50, "foo");

  int nothundred = 100;
  nothundred = 99;
  snprintf(x, nothundred, "foo");
}

void vargs_bad(va_list args) {
  char x[100];
  vsnprintf(x, sizeof(x), "bar", args); // expected-error {{Use VsprintfLiteral instead of vsnprintf when writing into a character array.}} expected-note {{This will prevent passing in the wrong size to vsnprintf accidentally.}}
  vsnprintf(x, 100, "bar", args); // expected-error {{Use VsprintfLiteral instead of vsnprintf when writing into a character array.}} expected-note {{This will prevent passing in the wrong size to vsnprintf accidentally.}}
  const int hundred = 100;
  vsnprintf(x, hundred, "bar", args); // expected-error {{Use VsprintfLiteral instead of vsnprintf when writing into a character array.}} expected-note {{This will prevent passing in the wrong size to vsnprintf accidentally.}}
}

void vargs_good(va_list args) {
  char x[100];
  int y;
  vsnprintf(x, sizeof(y), "foo", args);

  vsnprintf(x, 50, "foo", args);

  int nothundred = 100;
  nothundred = 99;
  vsnprintf(x, nothundred, "foo", args);
}
