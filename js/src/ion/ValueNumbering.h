/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
    static MDefinition *findSplit(MDefinition *);
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
    friend MDefinition *ValueNumberer::findSplit(MDefinition*);
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

