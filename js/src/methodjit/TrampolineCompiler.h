
#ifndef trampolines_h__
#define trampolines_h__

#include "assembler/jit/ExecutableAllocator.h"
#include "nunbox/Assembler.h"

namespace js {
namespace mjit {

class TrampolineCompiler
{
    typedef Assembler::Label Label;
    typedef Assembler::Jump Jump;
    typedef Assembler::ImmPtr ImmPtr;
    typedef Assembler::Address Address;
    typedef bool (*TrampolineGenerator)(Assembler &masm);

public:
    TrampolineCompiler(JSC::ExecutableAllocator *pool, Trampolines *tramps)
      : execPool(pool), trampolines(tramps)
    { }

    bool compile();
    static void release(Trampolines *tramps);

private:
    bool compileTrampoline(void **where, JSC::ExecutablePool **pool,
                           TrampolineGenerator generator);
    
    /* Generators for trampolines. */
    static bool generateForceReturn(Assembler &masm);

    JSC::ExecutableAllocator *execPool;
    Trampolines *trampolines;
};

}
}

#endif
