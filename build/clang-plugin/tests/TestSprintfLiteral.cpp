#include <cstdio>

void bad() {
  char x[100];
  snprintf(x, sizeof(x), "bar"); // expected-error {{Use SprintfLiteral instead of snprintf when writing into a character array.}} expected-note {{This will prevent passing in the wrong size to snprintf accidentally.}}
  snprintf(x, 100, "bar"); // expected-error {{Use SprintfLiteral instead of snprintf when writing into a character array.}} expected-note {{This will prevent passing in the wrong size to snprintf accidentally.}}
  snprintf(x, 101, "bar"); // expected-error {{Use SprintfLiteral instead of snprintf when writing into a character array.}} expected-note {{This will prevent passing in the wrong size to snprintf accidentally.}}
  const int hundred = 100;
  snprintf(x, hundred, "bar"); // expected-error {{Use SprintfLiteral instead of snprintf when writing into a character array.}} expected-note {{This will prevent passing in the wrong size to snprintf accidentally.}}
  const int hundredandone = 101;
  snprintf(x, hundredandone, "bar"); // expected-error {{Use SprintfLiteral instead of snprintf when writing into a character array.}} expected-note {{This will prevent passing in the wrong size to snprintf accidentally.}}
}

void ok() {
  char x[100];
  int y;
  snprintf(x, sizeof(y), "what");

  snprintf(x, 50, "what");

  int nothundred = 100;
  nothundred = 99;
  snprintf(x, nothundred, "what");
}

void vargs_bad(va_list args) {
  char x[100];
  vsnprintf(x, sizeof(x), "bar", args); // expected-error {{Use VsprintfLiteral instead of vsnprintf when writing into a character array.}} expected-note {{This will prevent passing in the wrong size to vsnprintf accidentally.}}
  vsnprintf(x, 100, "bar", args); // expected-error {{Use VsprintfLiteral instead of vsnprintf when writing into a character array.}} expected-note {{This will prevent passing in the wrong size to vsnprintf accidentally.}}
  vsnprintf(x, 101, "bar", args); // expected-error {{Use VsprintfLiteral instead of vsnprintf when writing into a character array.}} expected-note {{This will prevent passing in the wrong size to vsnprintf accidentally.}}
  const int hundred = 100;
  vsnprintf(x, hundred, "bar", args); // expected-error {{Use VsprintfLiteral instead of vsnprintf when writing into a character array.}} expected-note {{This will prevent passing in the wrong size to vsnprintf accidentally.}}
  const int hundredandone = 101;
  vsnprintf(x, hundredandone, "bar", args); // expected-error {{Use VsprintfLiteral instead of vsnprintf when writing into a character array.}} expected-note {{This will prevent passing in the wrong size to vsnprintf accidentally.}}
}

void vargs_good(va_list args) {
  char x[100];
  int y;
  vsnprintf(x, sizeof(y), "what", args);

  vsnprintf(x, 50, "what", args);

  int nothundred = 100;
  nothundred = 99;
  vsnprintf(x, nothundred, "what", args);
}
