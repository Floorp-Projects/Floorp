/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifdef _WIN32
#include "msvc_pragma.h"
#endif

#include <algorithm>
#include <list>
#include <map>
#include <stack>

#include "world.h"
#include "utilities.h"
#include "js2value.h"
#include "numerics.h"
#include "reader.h"
#include "parser.h"
#include "regexp.h"
#include "js2engine.h"
#include "bytecodecontainer.h"
#include "js2metadata.h"

namespace JavaScript {    
namespace MetaData {


    js2val Boolean_Constructor(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
    {   
        js2val thatValue = OBJECT_TO_JS2VAL(new (meta) BooleanInstance(meta, meta->booleanClass->prototype, meta->booleanClass));
        BooleanInstance *boolInst = checked_cast<BooleanInstance *>(JS2VAL_TO_OBJECT(thatValue));
        DEFINE_ROOTKEEPER(meta, rk, boolInst);

        if (argc > 0)
            boolInst->mValue = meta->toBoolean(argv[0]);
        else
            boolInst->mValue = false;
        return thatValue;
    }
    
    static js2val Boolean_Call(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
    {   
        if (argc > 0)
            return BOOLEAN_TO_JS2VAL(meta->toBoolean(argv[0]));
        else
            return JS2VAL_FALSE;
    }
    
    static js2val Boolean_toString(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
    {
        if (!JS2VAL_IS_OBJECT(thisValue) 
                || (JS2VAL_TO_OBJECT(thisValue)->kind != SimpleInstanceKind)
                || ((checked_cast<SimpleInstance *>(JS2VAL_TO_OBJECT(thisValue)))->type != meta->booleanClass))
            meta->reportError(Exception::typeError, "Boolean.toString called on something other than a boolean thing", meta->engine->errorPos());
        BooleanInstance *boolInst = checked_cast<BooleanInstance *>(JS2VAL_TO_OBJECT(thisValue));
        return (boolInst->mValue) ? meta->engine->allocString(meta->engine->true_StringAtom) : meta->engine->allocString(meta->engine->false_StringAtom);
    }

    static js2val Boolean_valueOf(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
    {
        if (!JS2VAL_IS_OBJECT(thisValue) 
                || (JS2VAL_TO_OBJECT(thisValue)->kind != SimpleInstanceKind)
                || ((checked_cast<SimpleInstance *>(JS2VAL_TO_OBJECT(thisValue)))->type != meta->booleanClass))
            meta->reportError(Exception::typeError, "Boolean.valueOf called on something other than a boolean thing", meta->engine->errorPos());
        BooleanInstance *boolInst = checked_cast<BooleanInstance *>(JS2VAL_TO_OBJECT(thisValue));
        return (boolInst->mValue) ? JS2VAL_TRUE : JS2VAL_FALSE;
    }

    void initBooleanObject(JS2Metadata *meta)
    {
        FunctionData prototypeFunctions[] =
        {
            { "toString",            0, Boolean_toString },
            { "valueOf",             0, Boolean_valueOf  },
            { NULL }
        };

        meta->initBuiltinClass(meta->booleanClass, NULL, Boolean_Constructor, Boolean_Call);
        meta->booleanClass->prototype = OBJECT_TO_JS2VAL(new (meta) BooleanInstance(meta, OBJECT_TO_JS2VAL(meta->objectClass->prototype), meta->booleanClass));
        meta->initBuiltinClassPrototype(meta->booleanClass, &prototypeFunctions[0]);
    }

}
}


