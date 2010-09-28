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
 * The Original Code is SpiderMonkey code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
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

#ifndef jscompartment_h___
#define jscompartment_h___

#include "jscntxt.h"
#include "jsgc.h"
#include "jsobj.h"
#include "jsfun.h"
#include "jsgcstats.h"
#include "jsclist.h"
#include "jsxml.h"

struct JSCompartment {
    JSRuntime       *rt;
    JSPrincipals    *principals;
    js::gc::Chunk   *chunk;

    js::gc::ArenaList<JSObject>      objArena;
    js::gc::ArenaList<JSFunction>    funArena;
    js::gc::ArenaList<JSShortString> shortStringArena;
    js::gc::ArenaList<JSString>      stringArena;
    js::gc::ArenaList<JSString>      externalStringArenas[js::gc::JS_EXTERNAL_STRING_LIMIT];
#if JS_HAS_XML_SUPPORT
    js::gc::ArenaList<JSXML>         xmlArena;
#endif

    js::gc::FreeLists                freeLists;
    
#ifdef JS_GCMETER
    js::gc::JSGCArenaStats compartmentStats[js::gc::FINALIZE_LIMIT];
#endif

    void *data;
    bool marked;
    js::WrapperMap crossCompartmentWrappers;
    bool debugMode;

    /* List all scripts in this compartment. */
    JSCList scripts;

    JSCompartment(JSRuntime *cx);
    ~JSCompartment();

    bool init();

    bool wrap(JSContext *cx, js::Value *vp);
    bool wrap(JSContext *cx, JSString **strp);
    bool wrap(JSContext *cx, JSObject **objp);
    bool wrapId(JSContext *cx, jsid *idp);
    bool wrap(JSContext *cx, js::PropertyOp *op);
    bool wrap(JSContext *cx, js::PropertyDescriptor *desc);
    bool wrap(JSContext *cx, js::AutoIdVector &props);
    bool wrapException(JSContext *cx);

    void sweep(JSContext *cx);
#ifdef JS_METHODJIT
    bool addScript(JSContext *cx, JSScript *script);
    void removeScript(JSScript *script);
#endif
    void purge(JSContext *cx);
    void finishArenaLists();
    bool arenaListsAreEmpty();
};

namespace js {

class PreserveCompartment {
  protected:
    JSContext *cx;
  private:
    JSCompartment *oldCompartment;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
  public:
     PreserveCompartment(JSContext *cx JS_GUARD_OBJECT_NOTIFIER_PARAM) : cx(cx) {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        oldCompartment = cx->compartment;
    }

    ~PreserveCompartment() {
        cx->compartment = oldCompartment;
    }
};

class SwitchToCompartment : public PreserveCompartment {
  public:
    SwitchToCompartment(JSContext *cx, JSCompartment *newCompartment) : PreserveCompartment(cx) {
        cx->compartment = newCompartment;
    }

    SwitchToCompartment(JSContext *cx, JSObject *target) : PreserveCompartment(cx) {
        cx->compartment = target->getCompartment(cx);
    }
};

}

#endif /* jscompartment_h___ */
