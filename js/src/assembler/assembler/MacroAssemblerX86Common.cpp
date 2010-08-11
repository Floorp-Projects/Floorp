#include "assembler/wtf/Platform.h"

#if WTF_CPU_X86 && !WTF_PLATFORM_MAC

#include "MacroAssemblerX86Common.h"

using namespace JSC;

MacroAssemblerX86Common::SSE2CheckState MacroAssemblerX86Common::s_sse2CheckState = NotCheckedSSE2;

#endif
