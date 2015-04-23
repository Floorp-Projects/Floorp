#define MOZ_IMPLICIT __attribute__((annotate("moz_implicit")))

struct Bad {
  operator bool(); // expected-error {{bad implicit conversion operator for 'Bad'}} expected-note {{consider adding the explicit keyword to 'operator bool'}}
};
struct Good {
  explicit operator bool();
};
struct Okay {
  MOZ_IMPLICIT operator bool();
};
