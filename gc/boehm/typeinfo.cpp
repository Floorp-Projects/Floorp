/*
  typeinfo.cpp
	
  Speculatively use RTTI on a random object. If it contains a pointer at offset 0
  that is in the current process' address space, and that so on, then attempt to
  use C++ RTTI's typeid operation to obtain the name of the type.
  
  by Patrick C. Beard.
 */

#include <typeinfo>
#include <ctype.h>

#include "gcconfig.h"

extern "C" const char* getTypeName(void* ptr);

class IUnknown {
public:
    virtual long QueryInterface() = 0;
    virtual long AddRef() = 0;
    virtual long Release() = 0;
};

#if defined(MACOS)

#include <Processes.h>

class AddressSpace {
public:
    AddressSpace();
    Boolean contains(void* ptr);
private:
    ProcessInfoRec mInfo;
};

AddressSpace::AddressSpace()
{
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    mInfo.processInfoLength = sizeof(mInfo);
    ::GetProcessInformation(&psn, &mInfo);
}

Boolean AddressSpace::contains(void* ptr)
{
    UInt32 start = UInt32(mInfo.processLocation);
    return (UInt32(ptr) >= start && UInt32(ptr) < (start + mInfo.processSize));
}

const char* getTypeName(void* ptr)
{
    // construct only one of these per process.
    static AddressSpace space;
	
    // sanity check the vtable pointer, before trying to use RTTI on the object.
    void** vt = *(void***)ptr;
    if (vt && !(unsigned(vt) & 0x3) && space.contains(vt) && space.contains(*vt)) {
	IUnknown* u = static_cast<IUnknown*>(ptr);
	const char* type = typeid(*u).name();
        // make sure it looks like a C++ identifier.
	if (type && (isalnum(type[0]) || type[0] == '_'))
	    return type;
    }
    return "void*";
}

#endif

#if defined(LINUX)

#include <signal.h>
#include <setjmp.h>

static jmp_buf context;

static void handler(int signum)
{
    longjmp(context, signum);
}

#define attempt() setjmp(context)

class Signaller {
public:
    Signaller(int signum);
    ~Signaller();

private:
    typedef void (*handler_t) (int signum);
    int mSignal;
    handler_t mOldHandler;
};

Signaller::Signaller(int signum)
    : mSignal(signum), mOldHandler(signal(signum, &handler))
{
}

Signaller::~Signaller()
{
    signal(mSignal, mOldHandler);
}

// The following are pointers that bamboozle our otherwise feeble
// attempts to "safely" collect type names.
//
// XXX this kind of sucks because it means that anyone trying to use
// this without NSPR will get unresolved symbols when this library
// loads. It's also not very extensible. Oh well: FIX ME!
extern "C" {
    // from nsprpub/pr/src/io/priometh.c (libnspr4.so)
    extern void* _pr_faulty_methods;
};

static inline int
sanity_check_vtable_i386(void** vt)
{
    // Now that we're "safe" inside the signal handler, we can
    // start poking around. If we're really an object with
    // RTTI, then the second entry in the vtable should point
    // to a function.
    //
    // Let's see if the second entry:
    //
    // 1) looks like a 4-byte aligned pointer
    //
    // 2) points to something that looks like the following
    //    i386 instructions:
    //
    //    55     push %ebp
    //    89e5   mov  %esp,%ebp
    //    53     push %ebx
    //
    //    or
    //
    //    55     push %ebp
    //    89e5   mov  %esp,%ebp
    //    56     push %esi
    //
    //    (which is the standard function prologue generated
    //    by egcs, plus a ``signature'' instruction that appears
    //    in the typeid() function's implementation).
    unsigned char** fp1 = reinterpret_cast<unsigned char**>(vt) + 1;

    // Does it look like an address?
    unsigned char* ip = *fp1;
    if ((unsigned(ip) & 3) != 0)
        return 0;

    // Does it look like it refers to the standard prologue?
    static unsigned char prologue[] = { 0x55, 0x89, 0xE5 };
    for (unsigned i = 0; i < sizeof(prologue); ++i)
	if (*ip++ != prologue[i])
	    return 0;

    // Is the next instruction a `push %ebx' or `push %esi'?
    return (*ip == 0x53 || *ip == 0x56);
}

static inline int
sanity_check_vtable_ppc(void** vt)
{
    // XXX write me!
    return 1;
}

#if defined(__i386)
#  define SANITY_CHECK_VTABLE(vt) (sanity_check_vtable_i386(vt))
#elif defined(PPC)
#  define SANITY_CHECK_VTABLE(vt) (sanity_check_vtable_ppc(vt))
#else
#  define SANITY_CHECK_VTABLE(vt) (1)
#endif

const char* getTypeName(void* ptr)
{
    // sanity check the vtable pointer, before trying to use RTTI on the object.
    void** vt = *(void***)ptr;
    if (vt && !(unsigned(vt) & 3) && (vt != &_pr_faulty_methods)) {
	Signaller s1(SIGSEGV);
	if (attempt() == 0) {
            if (SANITY_CHECK_VTABLE(vt)) {
                // Looks like a function: what the hell, let's call it.
                IUnknown* u = static_cast<IUnknown*>(ptr);
                const char* type = typeid(*u).name();
                // EGCS seems to prefix a length string.
                while (isdigit(*type)) ++type;
                return type;
	    }
	}
    }
    return "void*";
}

#endif
