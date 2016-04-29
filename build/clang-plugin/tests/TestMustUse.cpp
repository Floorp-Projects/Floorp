#define MOZ_MUST_USE_TYPE __attribute__((annotate("moz_must_use_type")))

struct Temporary { ~Temporary(); };
class MOZ_MUST_USE_TYPE MustUse {};
class MayUse {};

MustUse producesMustUse();
MustUse *producesMustUsePointer();
MustUse &producesMustUseRef();

MustUse producesMustUse(const Temporary& t);
MustUse *producesMustUsePointer(const Temporary& t);
MustUse &producesMustUseRef(const Temporary& t);

MayUse producesMayUse();
MayUse *producesMayUsePointer();
MayUse &producesMayUseRef();

void use(MustUse*);
void use(MustUse&);
void use(MustUse&&);
void use(MayUse*);
void use(MayUse&);
void use(MayUse&&);
void use(bool);

void foo() {
  MustUse u;

  producesMustUse(); // expected-error {{Unused value of must-use type 'MustUse'}}
  producesMustUsePointer();
  producesMustUseRef(); // expected-error {{Unused value of must-use type 'MustUse'}}
  producesMayUse();
  producesMayUsePointer();
  producesMayUseRef();
  u = producesMustUse();
  u = producesMustUse(Temporary());
  {
    producesMustUse(); // expected-error {{Unused value of must-use type 'MustUse'}}
    producesMustUsePointer();
    producesMustUseRef(); // expected-error {{Unused value of must-use type 'MustUse'}}
    producesMayUse();
    producesMayUsePointer();
    producesMayUseRef();
    u = producesMustUse();
    u = producesMustUse(Temporary());
  }
  if (true) {
    producesMustUse(); // expected-error {{Unused value of must-use type 'MustUse'}}
    producesMustUsePointer();
    producesMustUseRef(); // expected-error {{Unused value of must-use type 'MustUse'}}
    producesMayUse();
    producesMayUsePointer();
    producesMayUseRef();
    u = producesMustUse();
    u = producesMustUse(Temporary());
  } else {
    producesMustUse(); // expected-error {{Unused value of must-use type 'MustUse'}}
    producesMustUsePointer();
    producesMustUseRef(); // expected-error {{Unused value of must-use type 'MustUse'}}
    producesMayUse();
    producesMayUsePointer();
    producesMayUseRef();
    u = producesMustUse();
    u = producesMustUse(Temporary());
  }

  if(true) producesMustUse(); // expected-error {{Unused value of must-use type 'MustUse'}}
  else producesMustUse(); // expected-error {{Unused value of must-use type 'MustUse'}}
  if(true) producesMustUsePointer();
  else producesMustUsePointer();
  if(true) producesMustUseRef(); // expected-error {{Unused value of must-use type 'MustUse'}}
  else producesMustUseRef(); // expected-error {{Unused value of must-use type 'MustUse'}}
  if(true) producesMayUse();
  else producesMayUse();
  if(true) producesMayUsePointer();
  else producesMayUsePointer();
  if(true) producesMayUseRef();
  else producesMayUseRef();
  if(true) u = producesMustUse();
  else u = producesMustUse();
  if(true) u = producesMustUse(Temporary());
  else u = producesMustUse(Temporary());

  while (true) producesMustUse(); // expected-error {{Unused value of must-use type 'MustUse'}}
  while (true) producesMustUsePointer();
  while (true) producesMustUseRef(); // expected-error {{Unused value of must-use type 'MustUse'}}
  while (true) producesMayUse();
  while (true) producesMayUsePointer();
  while (true) producesMayUseRef();
  while (true) u = producesMustUse();
  while (true) u = producesMustUse(Temporary());

  do producesMustUse(); // expected-error {{Unused value of must-use type 'MustUse'}}
  while (true);
  do producesMustUsePointer();
  while (true);
  do producesMustUseRef(); // expected-error {{Unused value of must-use type 'MustUse'}}
  while (true);
  do producesMayUse();
  while (true);
  do producesMayUsePointer();
  while (true);
  do producesMayUseRef();
  while (true);
  do u = producesMustUse();
  while (true);
  do u = producesMustUse(Temporary());
  while (true);

  for (;;) producesMustUse(); // expected-error {{Unused value of must-use type 'MustUse'}}
  for (;;) producesMustUsePointer();
  for (;;) producesMustUseRef(); // expected-error {{Unused value of must-use type 'MustUse'}}
  for (;;) producesMayUse();
  for (;;) producesMayUsePointer();
  for (;;) producesMayUseRef();
  for (;;) u = producesMustUse();
  for (;;) u = producesMustUse(Temporary());

  for (producesMustUse();;); // expected-error {{Unused value of must-use type 'MustUse'}}
  for (producesMustUsePointer();;);
  for (producesMustUseRef();;); // expected-error {{Unused value of must-use type 'MustUse'}}
  for (producesMayUse();;);
  for (producesMayUsePointer();;);
  for (producesMayUseRef();;);
  for (u = producesMustUse();;);
  for (u = producesMustUse(Temporary());;);

  for (;;producesMustUse()); // expected-error {{Unused value of must-use type 'MustUse'}}
  for (;;producesMustUsePointer());
  for (;;producesMustUseRef()); // expected-error {{Unused value of must-use type 'MustUse'}}
  for (;;producesMayUse());
  for (;;producesMayUsePointer());
  for (;;producesMayUseRef());
  for (;;u = producesMustUse());
  for (;;u = producesMustUse(Temporary()));

  use((producesMustUse(), false)); // expected-error {{Unused value of must-use type 'MustUse'}}
  use((producesMustUsePointer(), false));
  use((producesMustUseRef(), false)); // expected-error {{Unused value of must-use type 'MustUse'}}
  use((producesMayUse(), false));
  use((producesMayUsePointer(), false));
  use((producesMayUseRef(), false));
  use((u = producesMustUse(), false));
  use((u = producesMustUse(Temporary()), false));

  switch (1) {
  case 1:
    producesMustUse(); // expected-error {{Unused value of must-use type 'MustUse'}}
    producesMustUsePointer();
    producesMustUseRef(); // expected-error {{Unused value of must-use type 'MustUse'}}
    producesMayUse();
    producesMayUsePointer();
    producesMayUseRef();
    u = producesMustUse();
    u = producesMustUse(Temporary());
  case 2:
    producesMustUse(); // expected-error {{Unused value of must-use type 'MustUse'}}
  case 3:
    producesMustUsePointer();
  case 4:
    producesMustUseRef(); // expected-error {{Unused value of must-use type 'MustUse'}}
  case 5:
    producesMayUse();
  case 6:
    producesMayUsePointer();
  case 7:
    producesMayUseRef();
  case 8:
    u = producesMustUse();
  case 9:
    u = producesMustUse(Temporary());
  default:
    producesMustUse(); // expected-error {{Unused value of must-use type 'MustUse'}}
    producesMustUsePointer();
    producesMustUseRef(); // expected-error {{Unused value of must-use type 'MustUse'}}
    producesMayUse();
    producesMayUsePointer();
    producesMayUseRef();
    u = producesMustUse();
    u = producesMustUse(Temporary());
  }

  use(producesMustUse());
  use(producesMustUsePointer());
  use(producesMustUseRef());
  use(producesMayUse());
  use(producesMayUsePointer());
  use(producesMayUseRef());
  use(u = producesMustUse());
  use(u = producesMustUse(Temporary()));

  MustUse a = producesMustUse();
  MustUse *b = producesMustUsePointer();
  MustUse &c = producesMustUseRef();
  MayUse d = producesMayUse();
  MayUse *e = producesMayUsePointer();
  MayUse &f = producesMayUseRef();
  MustUse g = u = producesMustUse();
  MustUse h = u = producesMustUse(Temporary());
}
