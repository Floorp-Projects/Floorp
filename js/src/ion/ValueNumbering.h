/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
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
                    uint32_t,
                    ValueHasher,
                    IonAllocPolicy> ValueMap;

    struct DominatingValue
    {
        MDefinition *def;
        uint32_t validUntil;
    };

    typedef HashMap<uint32_t,
                    DominatingValue,
                    DefaultHasher<uint32_t>,
                    IonAllocPolicy> InstructionMap;

  protected:
    uint32_t lookupValue(MDefinition *ins);
    MDefinition *findDominatingDef(InstructionMap &defs, MDefinition *ins, size_t index);

    MDefinition *simplify(MDefinition *def, bool useValueNumbers);
    MControlInstruction *simplifyControlInstruction(MControlInstruction *def);
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
    MIRGenerator *mir;
    MIRGraph &graph_;
    ValueMap values;
    bool pessimisticPass_;
    size_t count_;

  public:
    ValueNumberer(MIRGenerator *mir, MIRGraph &graph, bool optimistic);
    bool analyze();
};

class ValueNumberData : public TempObject {

    friend void ValueNumberer::breakClass(MDefinition*);
    friend MDefinition *ValueNumberer::findSplit(MDefinition*);
    uint32_t number;
    MDefinition *classNext;
    MDefinition *classPrev;

  public:
    ValueNumberData() : number(0), classNext(NULL), classPrev(NULL) {}

    void setValueNumber(uint32_t number_) {
        number = number_;
    }

    uint32_t valueNumber() {
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

