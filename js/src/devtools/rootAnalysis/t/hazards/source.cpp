#define ANNOTATE(property) __attribute__((tag(property)))

struct Cell { int f; } ANNOTATE("GC Thing");

class AutoSuppressGC_Base {
  public:
    AutoSuppressGC_Base() {}
    ~AutoSuppressGC_Base() {}
} ANNOTATE("Suppress GC");

class AutoSuppressGC_Child : public AutoSuppressGC_Base {
  public:
    AutoSuppressGC_Child() : AutoSuppressGC_Base() {}
};

class AutoSuppressGC {
    AutoSuppressGC_Child helpImBeingSuppressed;

  public:
    AutoSuppressGC() {}
};

extern void GC() ANNOTATE("GC Call");
extern void invisible();

void GC()
{
    // If the implementation is too trivial, the function body won't be emitted at all.
    asm("");
    invisible();
}

extern void foo(Cell*);

void suppressedFunction() {
    GC(); // Calls GC, but is always called within AutoSuppressGC
}

void halfSuppressedFunction() {
    GC(); // Calls GC, but is sometimes called within AutoSuppressGC
}

void unsuppressedFunction() {
    GC(); // Calls GC, never within AutoSuppressGC
}

volatile static int x = 3;
volatile static int* xp = &x;
struct GCInDestructor {
    ~GCInDestructor() {
        invisible();
        asm("");
        *xp = 4;
        GC();
    }
};

Cell*
f()
{
    GCInDestructor kaboom;

    Cell cell;
    Cell* cell1 = &cell;
    Cell* cell2 = &cell;
    Cell* cell3 = &cell;
    Cell* cell4 = &cell;
    {
        AutoSuppressGC nogc;
        suppressedFunction();
        halfSuppressedFunction();
    }
    foo(cell1);
    halfSuppressedFunction();
    foo(cell2);
    unsuppressedFunction();
    {
        // Old bug: it would look from the first AutoSuppressGC constructor it
        // found to the last destructor. This statement *should* have no effect.
        AutoSuppressGC nogc;
    }
    foo(cell3);
    Cell* cell5 = &cell;
    foo(cell5);

    // Hazard in return value due to ~GCInDestructor
    Cell* cell6 = &cell;
    return cell6;
}
