#include "MMgc.h"

using namespace std;

#ifdef OVERRIDE_GLOBAL_NEW

#ifdef _DEBUG
int cBlocksAllocated = 0;  // Count of blocks allocated.
#endif // _DEBUG

#ifndef __GNUC__

// User-defined operator new.
void *operator new(size_t size)
{
	#ifdef _DEBUG
		cBlocksAllocated++;
	#endif // _DEBUG

	return MMgc::FixedMalloc::GetInstance()->Alloc(size);
}

void *operator new[](size_t size)
{
	#ifdef _DEBUG
		cBlocksAllocated++;
	#endif

	return MMgc::FixedMalloc::GetInstance()->Alloc(size);
}

// User-defined operator delete.
#ifdef _MAC
// CW9 wants the C++ official prototype, which means we must have an empty exceptions list for throw.
// (The fact exceptions aren't on doesn't matter.) - mds, 02/05/04
void operator delete( void *p) throw()
#else
void operator delete( void *p)
#endif
{
	MMgc::FixedMalloc::GetInstance()->Free(p);
	#ifdef _DEBUG
		cBlocksAllocated--;
	#endif // _DEBUG
}

#ifdef _MAC
// CW9 wants the C++ official prototype, which means we must have an empty exceptions list for throw.
// (The fact exceptions aren't on doesn't matter.) - mds, 02/05/04
void operator delete[]( void *p) throw()
#else
void operator delete[]( void *p )
#endif
{
	MMgc::FixedMalloc::GetInstance()->Free(p);
	#ifdef _DEBUG
		cBlocksAllocated--;
	#endif // _DEBUG
}

#endif // __GNUC__
#endif // OVERRIDE_GLOBAL_NEW
