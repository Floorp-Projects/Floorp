/*
	GC_profiler.cpp
	
	Experimental hack to clear stack frames upon entry, to make a conservative
	garbage collector more accurate.
 */

#include <Profiler.h>

#pragma profile off

/**
 * Instruction constants. To clear the stack frame accurately,
 * we "interpret" the caller's prologue to see which stack locations
 * are already in use to save non-volatile registers. this is pretty
 * specific to Metrowerks generated PowerPC code.
 */
const unsigned MFLR_R0 = 0x7C0802A6;
const unsigned STW = 0x24;				// 0b100100 == 0x24
const unsigned STMW = 0x2F;				// 0b101111 == 0x2F
const unsigned R1 = 0x01;

/**
 * Multiplicative hash, from Knuth 6.4.
 */
const unsigned GOLDEN_RATIO = 0x9E3779B9U;

/**
 * A PowerPC store instruction has this form.
 */
struct store {
	unsigned opcode : 6;
	unsigned source : 5;
	unsigned destination : 5;
	signed offset : 16;
};

struct stack_frame {
	stack_frame*	next;				// savedSP
	void*			savedCR;
	void*			savedLR;
	void*			reserved0;
	void*			reserved1;
	void*			savedTOC;
};

struct offset_cache {
    void*           savedLR;
    signed          min_offset;
};

asm stack_frame* getStackFrame()
{
	mr	r3,sp
	blr
}

static unsigned long long hits = 1, misses = 1;
static double hit_ratio = 1.0;

static const CACHE_BITS = 12;
static const CACHE_SIZE = (1 << CACHE_BITS);    // 4096 elements should be enough.
static const CACHE_SHIFT = (32 - CACHE_BITS);   // only consider upper CACHE_BITS of the hashed value.
static const CACHE_MASK = (CACHE_SIZE - 1);     // only consider lower CACHE_BITS of the hashed value.

static struct cache_entry {
    void*       savedLR;
    signed      minOffset;
    const char* functionName;
} offset_cache[CACHE_SIZE];

/**
 * This routine clears the newly reserved stack space in the caller's frame.
 * This is done so that the stack won't contain any garbage pointers which
 * can cause undue retention of garbage.
 */
pascal void __PROFILE_ENTRY(char* functionName)
{
	stack_frame* myLinkage = getStackFrame()->next;
	stack_frame* callersLinkage = myLinkage->next;

    signed minOffset = 0;

    // see if we've scanned this routine before, by consulting a direct-mapped cache.
    // we're just hashing the caller's address, using the Knuth's venerable multiplicative
    // hash function.
    void* savedLR = myLinkage->savedLR;
    unsigned h = ((reinterpret_cast<unsigned>(savedLR) >> 2) * GOLDEN_RATIO) >> CACHE_SHIFT;
    // unsigned h = ((reinterpret_cast<unsigned>(savedLR) >> 2) * GOLDEN_RATIO) & CACHE_MASK;
    cache_entry& entry = offset_cache[h];
    
    if (entry.savedLR == savedLR) {
	    minOffset = entry.minOffset;
	    // ++hits;
	} else {
		// scan the caller's prologue, looking for stw instructions. stores to negative
		// offsets off of sp will tell us where the beginning of the unitialized
		// frame begins. stmw instructions are also considered.
		// start scanning at the first instruction before the call.
		unsigned* pc = -2 + reinterpret_cast<unsigned*>(savedLR);
		while (*pc != MFLR_R0) {
			store* instruction = reinterpret_cast<store*>(pc--);
			unsigned opcode = instruction->opcode;
			if ((opcode == STW || opcode == STMW) && instruction->destination == R1) {
				signed offset = instruction->offset;
				if (offset < minOffset)
					minOffset = offset;
			}
		}

		// cache the minimum offset value, to avoid rescanning the instructions on
		// subsequent invocations.
        entry.savedLR = savedLR;
        entry.minOffset = minOffset;
        entry.functionName = functionName;
        // ++misses;
	}
    // hit_ratio = (double)hits / (double)misses;

	// clear the space between the linkage area the caller has reserved
	// for us, and the caller's saved non-volatile registers.
	signed* ptr = reinterpret_cast<signed*>(myLinkage + 1);
	signed* end = reinterpret_cast<signed*>(minOffset + reinterpret_cast<char*>(callersLinkage));
	while (ptr < end) *ptr++ = 0;
}

pascal void __PROFILE_EXIT()
{
}
