/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

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


    static js2val Number_Constructor(JS2Metadata *meta, const js2val thisValue, js2val argv[], uint32 argc)
    {   
        js2val thatValue = OBJECT_TO_JS2VAL(new NumberInstance(meta->numberClass));
        NumberInstance *numInst = checked_cast<NumberInstance *>(JS2VAL_TO_OBJECT(thatValue));

        if (argc > 0)
            numInst->mValue = meta->toFloat64(argv[0]);
        else
            numInst->mValue = 0.0;
        return thatValue;
    }
    
    static js2val Number_toString(JS2Metadata *meta, const js2val thisValue, js2val * /*argv*/, uint32 /*argc*/)
    {
        if (meta->objectType(thisValue) != meta->numberClass)
            meta->reportError(Exception::typeError, "Number.toString called on something other than a number thing", meta->engine->errorPos());
        NumberInstance *numInst = checked_cast<NumberInstance *>(JS2VAL_TO_OBJECT(thisValue));
        return STRING_TO_JS2VAL(meta->engine->numberToString(&numInst->mValue));
    }

#define MAKE_INSTANCE_VARIABLE(name, type) \
    m = new InstanceVariable(type, false, false, meta->numberClass->slotCount++); \
    meta->defineInstanceMember(meta->numberClass, &meta->cxt, &meta->world.identifiers[name], &publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, m, 0);

    void initNumberObject(JS2Metadata *meta)
    {
        meta->numberClass->construct = Number_Constructor;

#define N_CONSTANTS_COUNT (5)

        struct {
            char *name;
            float64 value;
        } NumberObjectConstants[N_CONSTANTS_COUNT] = {
            { "MAX_VALUE", maxDouble },
            { "MIN_VALUE", minDouble },
            { "NaN", nan },
            { "POSITIVE_INFINITY", positiveInfinity },
            { "NEGATIVE_INFINITY", negativeInfinity },
        };

        NamespaceList publicNamespaceList;
        publicNamespaceList.push_back(meta->publicNamespace);

        uint32 i;
        meta->env.addFrame(meta->numberClass);
        for (i = 0; i < N_CONSTANTS_COUNT; i++)
        {
            Variable *v = new Variable(meta->numberClass, meta->engine->allocNumber(NumberObjectConstants[i].value), true);
            meta->defineStaticMember(&meta->env, &meta->world.identifiers[NumberObjectConstants[i].name], &publicNamespaceList, Attribute::NoOverride, false, ReadWriteAccess, v, 0);
        }
        meta->env.removeTopFrame();

    }



}
}


