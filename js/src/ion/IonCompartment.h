/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
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

#ifndef jsion_ion_compartment_h__
#define jsion_ion_compartment_h__

#include "IonCode.h"
#include "jsvalue.h"

namespace js {
namespace ion {

typedef void * CalleeToken;

typedef JSBool (*EnterIonCode)(void *code, int argc, Value *argv, Value *vp,
                               CalleeToken calleeToken);

class IonCompartment {
    JSC::ExecutableAllocator *execAlloc_;

    // Trampolines.
    IonCode *enterJIT_;

    IonCode *generateEnterJIT(JSContext *cx);

  public:
    bool initialize(JSContext *cx);
    IonCompartment();
    ~IonCompartment();

    void mark(JSTracer *trc, JSCompartment *compartment);
    void sweep(JSContext *cx);

    JSC::ExecutableAllocator *execAlloc() {
        return execAlloc_;
    }

    EnterIonCode enterJIT(JSContext *cx) {
        if (!enterJIT_) {
            enterJIT_ = generateEnterJIT(cx);
            if (!enterJIT_)
                return NULL;
        }
        return enterJIT_->as<EnterIonCode>();
    }
};


} // namespace js
} // namespace ion

#endif // jsion_ion_compartment_h__

