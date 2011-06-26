/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is the Mozilla SpiderMonkey JaegerMonkey implementation
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2002-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "jsautooplen.h"

namespace js {

class BytecodeRange {
  public:
    BytecodeRange(JSContext *cx, JSScript *script)
      : cx(cx), script(script), pc(script->code), end(pc + script->length) {}
    bool empty() const { return pc == end; }
    jsbytecode *frontPC() const { return pc; }
    JSOp frontOpcode() const { return js_GetOpcode(cx, script, pc); }
    size_t frontOffset() const { return pc - script->code; }
    void popFront() { pc += GetBytecodeLength(cx, script, pc); }

  private:
    JSContext *cx;
    JSScript *script;
    jsbytecode *pc, *end;
};

/* 
 * Warning: this does not skip JSOP_RESETBASE* or JSOP_INDEXBASE* ops, so it is
 * useful only when checking for optimization opportunities.
 */
JS_ALWAYS_INLINE jsbytecode *
AdvanceOverBlockchainOp(jsbytecode *pc)
{
    if (*pc == JSOP_NULLBLOCKCHAIN)
        return pc + JSOP_NULLBLOCKCHAIN_LENGTH;
    if (*pc == JSOP_BLOCKCHAIN)
        return pc + JSOP_BLOCKCHAIN_LENGTH;
    return pc;
}

}
