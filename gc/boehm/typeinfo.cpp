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

const char* getTypeName(void* ptr)
{
    // sanity check the vtable pointer, before trying to use RTTI on the object.
    void** vt = *(void***)ptr;
    if (vt && !(unsigned(vt) & 3)) {
	Signaller signaller(SIGSEGV);
	if (attempt() == 0) {
	    IUnknown* u = static_cast<IUnknown*>(ptr);
	    const char* type = typeid(*u).name();
	    // make sure it looks like a C++ identifier.
	    if (type && (isalnum(type[0]) || type[0] == '_')) {
		// ECGS seems to prefix a length string.
		while (isdigit(*type)) ++type;
		return type;
	    }
	} 
    }
    return "void*";
}

#endif
