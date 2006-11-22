#ifndef __GCGLOBALNEW__
#define __GCGLOBALNEW__

#ifdef OVERRIDE_GLOBAL_NEW
#ifdef __GNUC__
// Custom new and delete operators

#ifdef _DEBUG
extern int cBlocksAllocated;
#endif // _DEBUG

// User-defined operator new.
inline __attribute__((always_inline)) void *operator new(size_t size)
{
    #ifdef _DEBUG
	cBlocksAllocated++;
    #endif // _DEBUG
       
	return MMgc::FixedMalloc::GetInstance()->Alloc(size);
}
       
inline __attribute__((always_inline)) void *operator new[](size_t size)
{
    #ifdef _DEBUG
    cBlocksAllocated++;
    #endif // _DEBUG
       
	return MMgc::FixedMalloc::GetInstance()->Alloc(size);
}
       
// User-defined operator delete.
inline __attribute__((always_inline)) void operator delete( void *p)
{
	MMgc::FixedMalloc::GetInstance()->Free(p);
       
    #ifdef _DEBUG
	cBlocksAllocated--;
    #endif // _DEBUG
}
       
inline __attribute__((always_inline)) void operator delete[]( void *p )
{
	MMgc::FixedMalloc::GetInstance()->Free(p);
       
    #ifdef _DEBUG
	cBlocksAllocated--;
    #endif // _DEBUG
}

#endif // __GNUC__
#endif // OVERRIDE_GLOBAL_NEW

#endif // __GCGLOBALNEW__
