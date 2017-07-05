#define MOZ_NO_DANGLING_ON_TEMPORARIES                                              \
  __attribute__((annotate("moz_no_dangling_on_temporaries")))

class AnnotateConflict {
  MOZ_NO_DANGLING_ON_TEMPORARIES int *get() && { return nullptr; } // expected-error {{methods annotated with MOZ_NO_DANGLING_ON_TEMPORARIES cannot be && ref-qualified}}
  MOZ_NO_DANGLING_ON_TEMPORARIES int test() { return 0; } // expected-error {{methods annotated with MOZ_NO_DANGLING_ON_TEMPORARIES must return a pointer}}
};

class NS_ConvertUTF8toUTF16 {
public:
  MOZ_NO_DANGLING_ON_TEMPORARIES int *get() { return nullptr; }
};

NS_ConvertUTF8toUTF16 TemporaryFunction() { return NS_ConvertUTF8toUTF16(); }

void UndefinedFunction(int* test);

void NoEscapeFunction(int *test) {}

int *glob; // expected-note {{through the variable declared here}}
void EscapeFunction1(int *test) { glob = test; } // expected-note {{the raw pointer escapes the function scope here}}

void EscapeFunction2(int *test, int *&escape) { escape = test; } // expected-note {{the raw pointer escapes the function scope here}} \
                                                                    expected-note {{through the parameter declared here}}

int *EscapeFunction3(int *test) { return test; } // expected-note {{the raw pointer escapes the function scope here}} \
                                                    expected-note {{through the return value of the function declared here}}

int main() {
  int *test = TemporaryFunction().get(); // expected-error {{calling `get` on a temporary, potentially allowing use after free of the raw pointer}}
  int *test2 = NS_ConvertUTF8toUTF16().get(); // expected-error {{calling `get` on a temporary, potentially allowing use after free of the raw pointer}}

  UndefinedFunction(NS_ConvertUTF8toUTF16().get());

  NoEscapeFunction(TemporaryFunction().get());
  EscapeFunction1(TemporaryFunction().get()); // expected-error {{calling `get` on a temporary, potentially allowing use after free of the raw pointer}}

  int *escape;
  EscapeFunction2(TemporaryFunction().get(), escape); // expected-error {{calling `get` on a temporary, potentially allowing use after free of the raw pointer}}
  int *escape2 = EscapeFunction3(TemporaryFunction().get()); // expected-error {{calling `get` on a temporary, potentially allowing use after free of the raw pointer}}
}
