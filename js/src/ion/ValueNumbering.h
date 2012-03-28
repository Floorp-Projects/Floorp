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
 *   Ryan Pearl <rpearl@mozilla.com>
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

#ifndef jsion_value_numbering_h__
#define jsion_value_numbering_h__

#include "MIR.h"
#include "MIRGraph.h"
#include "CompileInfo.h"

namespace js {
namespace ion {

class ValueNumberer
{
  protected:
    struct ValueHasher
    {
        typedef MDefinition * Lookup;
        typedef MDefinition * Key;
        static HashNumber hash(const Lookup &ins) {
            return ins->valueHash();
        }

        static bool match(const Key &k, const Lookup &l) {
            // If one of the instructions depends on a store, and the
            // other instruction does not depend on the same store,
            // the instructions are not congruent.
            if (k->dependency() != l->dependency())
                return false;
            return k->congruentTo(l);
        }
    };

    typedef HashMap<MDefinition *,
                    uint32,
                    ValueHasher,
                    IonAllocPolicy> ValueMap;

    struct DominatingValue
    {
        MDefinition *def;
        uint32 validUntil;
    };

    typedef HashMap<uint32,
                    DominatingValue,
                    DefaultHasher<uint32>,
                    IonAllocPolicy> InstructionMap;

  protected:
    uint32 lookupValue(MDefinition *ins);
    MDefinition *findDominatingDef(InstructionMap &defs, MDefinition *ins, size_t index);

    MDefinition *simplify(MDefinition *def, bool useValueNumbers);
    bool eliminateRedundancies();

    bool computeValueNumbers();

    inline bool isMarked(MDefinition *def) {
        return pessimisticPass_ || def->isInWorklist();
    }

    void markDefinition(MDefinition *def);
    void unmarkDefinition(MDefinition *def);

    void markConsumers(MDefinition *def);
    void markBlock(MBasicBlock *block);
    void setClass(MDefinition *toSet, MDefinition *representative);

  public:
    static bool needsSplit(MDefinition *);
    void breakClass(MDefinition*);

  protected:
    MIRGraph &graph_;
    ValueMap values;
    bool pessimisticPass_;
    size_t count_;

  public:
    ValueNumberer(MIRGraph &graph, bool optimistic);
    bool analyze();
};

class ValueNumberData : public TempObject {

    friend void ValueNumberer::breakClass(MDefinition*);
    friend bool ValueNumberer::needsSplit(MDefinition*);
    uint32 number;
    MDefinition *classNext;
    MDefinition *classPrev;

  public:
    ValueNumberData() : number(0), classNext(NULL), classPrev(NULL) {}

    void setValueNumber(uint32 number_) {
        number = number_;
    }

    uint32 valueNumber() {
        return number;
    }
    // Set the class of this to the given representative value.
    void setClass(MDefinition *thisDef, MDefinition *rep) {
        JS_ASSERT(thisDef->valueNumberData() == this);
        // If this value should already be in the given set, don't do anything
        if (number == rep->valueNumber())
            return;

        if (classNext)
            classNext->valueNumberData()->classPrev = classPrev;
        if (classPrev)
            classPrev->valueNumberData()->classNext = classNext;


        classPrev = rep;
        classNext = rep->valueNumberData()->classNext;

        if (rep->valueNumberData()->classNext)
            rep->valueNumberData()->classNext->valueNumberData()->classPrev = thisDef;
        rep->valueNumberData()->classNext = thisDef;
    }
};
} // namespace ion
} // namespace js

#endif // jsion_value_numbering_h__

