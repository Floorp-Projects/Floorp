#define ANNOTATE(property) __attribute__((annotate(property)))

struct Cell { int f; } ANNOTATE("GC Thing");

extern void GC() ANNOTATE("GC Call");

void GC()
{
    // If the implementation is too trivial, the function body won't be emitted at all.
    asm("");
}

class RAII_GC {
  public:
    RAII_GC() { }
    ~RAII_GC() { GC(); }
};

// ~AutoSomething calls GC because of the RAII_GC field. The constructor,
// though, should *not* GC -- unless it throws an exception. Which is not
// possible when compiled with -fno-exceptions. This test will try it both
// ways.
class AutoSomething {
    RAII_GC gc;
  public:
    AutoSomething() : gc() {
        asm(""); // Ooh, scary, this might throw an exception
    }
    ~AutoSomething() {
        asm("");
    }
};

extern void usevar(Cell* cell);

void f() {
    Cell* thing = nullptr; // Live range starts here

    // When compiling with -fexceptions, there should be a hazard below. With
    // -fno-exceptions, there should not be one. We will check both.
    {
        AutoSomething smth; // Constructor can GC only if exceptions are enabled
        usevar(thing); // Live range ends here
    } // In particular, 'thing' is dead at the destructor, so no hazard
}
