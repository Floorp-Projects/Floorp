/*
	typeinfo.cpp
	
	Speculatively use RTTI on a random object. If it contains a pointer at offset 0
	that is in the current process' address space, and that so on, then attempt to
	use C++ RTTI's typeid operation to obtain the name of the type.
	
	by Patrick C. Beard.
 */

#include <typeinfo>
#include <ctype.h>
#include <Processes.h>

extern "C" const char* getTypeName(void* ptr);

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

class IUnknown {
	virtual long QueryInterface() = 0;
	virtual long AddRef() = 0;
	virtual long Release() = 0;
};

const char* getTypeName(void* ptr)
{
	// construct only one of these per process.
	static AddressSpace space;
	
	// sanity check the vtable pointer, before trying to use RTTI on the object.
	void** vt = *(void***)ptr;
	if (space.contains(vt) && space.contains(*vt)) {
		IUnknown* u = static_cast<IUnknown*>(ptr);
		const char* type = typeid(*u).name();
        // make sure it looks like a C++ identifier.
		if (type && (isalnum(type[0]) || type[0] == '_'))
		    return type;
	}
	return "void*";
}
