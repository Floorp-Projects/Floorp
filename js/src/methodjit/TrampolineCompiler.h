/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Jaegermonkey.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 *
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Andrew Drake <drakedevel@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#if !defined trampolines_h__ && defined JS_METHODJIT
#define trampolines_h__

#include "assembler/jit/ExecutableAllocator.h"
#include "methodjit/CodeGenIncludes.h"

namespace js {
namespace mjit {

class TrampolineCompiler
{
    typedef Assembler::Label Label;
    typedef Assembler::Jump Jump;
    typedef Assembler::ImmPtr ImmPtr;
    typedef Assembler::Imm32 Imm32;
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

#if (defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)) || defined(_WIN64)
    static bool generateForceReturnFast(Assembler &masm);
#endif

    JSC::ExecutableAllocator *execPool;
    Trampolines *trampolines;
};

} /* namespace mjit */
} /* namespace js */

#endif

