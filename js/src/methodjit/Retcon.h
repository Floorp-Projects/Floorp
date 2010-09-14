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

/*
 * Retroactive continuity ("retcon") refers to the retroactive modification
 * or reinterpretation of established facts. 
 */

#if !defined jsjaeger_retcon_h__ && defined JS_METHODJIT
#define jsjaeger_retcon_h__

#include "jscntxt.h"
#include "jsscript.h"
#include "MethodJIT.h"
#include "Compiler.h"

namespace js {
namespace mjit {

/*
 * A problem often arises where, for one reason or another, a piece of code
 * wants to touch the script->code, but isn't expecting JSOP_TRAP. This allows
 * one to temporarily remove JSOP_TRAPs from the instruction stream (without
 * copying) and automatically re-add them on scope exit.
 */
class AutoScriptRetrapper
{
  public:
    AutoScriptRetrapper(JSContext *cx1, JSScript *script1) :
        cx(cx1), script(script1), traps(cx) {};
    ~AutoScriptRetrapper();

    bool untrap(jsbytecode *pc);

  private:
    JSContext *cx;
    JSScript *script;
    Vector<jsbytecode*> traps;
};

/*
 * This class is responsible for sanely re-JITing a script and fixing up
 * the world. If you ever change the code associated with a JSScript, or
 * otherwise would cause existing JITed code to be incorrect, you /must/ use
 * this to invalidate and potentially re-compile the existing JITed code,
 * fixing up the stack in the process.
 */
class Recompiler {
    struct PatchableAddress {
        void **location;
        CallSite callSite;
    };
    
public:
    Recompiler(JSContext *cx, JSScript *script);
    
    bool recompile();

private:
    JSContext *cx;
    JSScript *script;
    
    PatchableAddress findPatch(void **location);
    void applyPatch(Compiler& c, PatchableAddress& toPatch);
};

} /* namespace mjit */
} /* namespace js */

#endif

