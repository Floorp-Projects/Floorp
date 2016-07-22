/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/AliasAnalysisShared.h"

#include "jit/MIR.h"

namespace js {
namespace jit {

void
AliasAnalysisShared::spewDependencyList()
{
#ifdef JS_JITSPEW
    if (JitSpewEnabled(JitSpew_AliasSummaries)) {
        Fprinter &print = JitSpewPrinter();
        JitSpewHeader(JitSpew_AliasSummaries);
        print.printf("Dependency list for other passes:\n");

        for (ReversePostorderIterator block(graph_.rpoBegin()); block != graph_.rpoEnd(); block++) {
            for (MInstructionIterator def(block->begin()), end(block->begin(block->lastIns()));
                 def != end;
                 ++def)
            {
                if (!def->dependency())
                    continue;
                if (!def->getAliasSet().isLoad())
                    continue;

                JitSpewHeader(JitSpew_AliasSummaries);
                print.printf(" ");
                MDefinition::PrintOpcodeName(print, def->op());
                print.printf("%d marked depending on ", def->id());
                MDefinition::PrintOpcodeName(print, def->dependency()->op());
                print.printf("%d\n", def->dependency()->id());
            }
        }
    }
#endif
}

// Unwrap any slot or element to its corresponding object.
static inline const MDefinition*
MaybeUnwrap(const MDefinition* object)
{

    while (object->isSlots() || object->isElements() || object->isConvertElementsToDoubles()) {
        MOZ_ASSERT(object->numOperands() == 1);
        object = object->getOperand(0);
    }

    if (object->isTypedArrayElements())
        return nullptr;
    if (object->isTypedObjectElements())
        return nullptr;
    if (object->isConstantElements())
        return nullptr;

    return object;
}

// Get the object of any load/store. Returns nullptr if not tied to
// an object.
static inline const MDefinition*
GetObject(const MDefinition* ins)
{
    if (!ins->getAliasSet().isStore() && !ins->getAliasSet().isLoad())
        return nullptr;

    const MDefinition* object = nullptr;
    switch (ins->op()) {
      case MDefinition::Op_GetPropertyCache:
        if (!ins->toGetPropertyCache()->idempotent())
            return nullptr;
        object = ins->getOperand(0);
        break;
      case MDefinition::Op_InitializedLength:
      case MDefinition::Op_LoadElement:
      case MDefinition::Op_LoadUnboxedScalar:
      case MDefinition::Op_LoadUnboxedObjectOrNull:
      case MDefinition::Op_LoadUnboxedString:
      case MDefinition::Op_StoreElement:
      case MDefinition::Op_StoreUnboxedObjectOrNull:
      case MDefinition::Op_StoreUnboxedString:
      case MDefinition::Op_StoreUnboxedScalar:
      case MDefinition::Op_SetInitializedLength:
      case MDefinition::Op_ArrayLength:
      case MDefinition::Op_SetArrayLength:
      case MDefinition::Op_StoreElementHole:
      case MDefinition::Op_TypedObjectDescr:
      case MDefinition::Op_Slots:
      case MDefinition::Op_Elements:
      case MDefinition::Op_MaybeCopyElementsForWrite:
      case MDefinition::Op_MaybeToDoubleElement:
      case MDefinition::Op_UnboxedArrayLength:
      case MDefinition::Op_UnboxedArrayInitializedLength:
      case MDefinition::Op_IncrementUnboxedArrayInitializedLength:
      case MDefinition::Op_SetUnboxedArrayInitializedLength:
      case MDefinition::Op_TypedArrayLength:
      case MDefinition::Op_SetTypedObjectOffset:
      case MDefinition::Op_SetDisjointTypedElements:
      case MDefinition::Op_ArrayPopShift:
      case MDefinition::Op_ArrayPush:
      case MDefinition::Op_ArraySlice:
      case MDefinition::Op_LoadTypedArrayElementHole:
      case MDefinition::Op_StoreTypedArrayElementHole:
      case MDefinition::Op_LoadFixedSlot:
      case MDefinition::Op_LoadFixedSlotAndUnbox:
      case MDefinition::Op_StoreFixedSlot:
      case MDefinition::Op_GetPropertyPolymorphic:
      case MDefinition::Op_SetPropertyPolymorphic:
      case MDefinition::Op_GuardShape:
      case MDefinition::Op_GuardReceiverPolymorphic:
      case MDefinition::Op_GuardObjectGroup:
      case MDefinition::Op_GuardObjectIdentity:
      case MDefinition::Op_GuardClass:
      case MDefinition::Op_GuardUnboxedExpando:
      case MDefinition::Op_LoadUnboxedExpando:
      case MDefinition::Op_LoadSlot:
      case MDefinition::Op_StoreSlot:
      case MDefinition::Op_InArray:
      case MDefinition::Op_LoadElementHole:
      case MDefinition::Op_TypedArrayElements:
      case MDefinition::Op_TypedObjectElements:
        object = ins->getOperand(0);
        break;
      case MDefinition::Op_LoadTypedArrayElementStatic:
      case MDefinition::Op_StoreTypedArrayElementStatic:
      case MDefinition::Op_GetDOMProperty:
      case MDefinition::Op_GetDOMMember:
      case MDefinition::Op_Call:
      case MDefinition::Op_Compare:
      case MDefinition::Op_GetArgumentsObjectArg:
      case MDefinition::Op_SetArgumentsObjectArg:
      case MDefinition::Op_GetFrameArgument:
      case MDefinition::Op_SetFrameArgument:
      case MDefinition::Op_CompareExchangeTypedArrayElement:
      case MDefinition::Op_AtomicExchangeTypedArrayElement:
      case MDefinition::Op_AtomicTypedArrayElementBinop:
      case MDefinition::Op_AsmJSLoadHeap:
      case MDefinition::Op_AsmJSStoreHeap:
      case MDefinition::Op_WasmLoad:
      case MDefinition::Op_WasmStore:
      case MDefinition::Op_AsmJSCompareExchangeHeap:
      case MDefinition::Op_AsmJSAtomicBinopHeap:
      case MDefinition::Op_WasmLoadGlobalVar:
      case MDefinition::Op_WasmStoreGlobalVar:
      case MDefinition::Op_ArrayJoin:
        return nullptr;
      default:
#ifdef DEBUG
        // Crash when the default aliasSet is overriden, but when not added in the list above.
        if (!ins->getAliasSet().isStore() || ins->getAliasSet().flags() != AliasSet::Flag::Any)
            MOZ_CRASH("Overridden getAliasSet without updating AliasAnalysisShared GetObject");
#endif

        return nullptr;
    }

    MOZ_ASSERT(!ins->getAliasSet().isStore() || ins->getAliasSet().flags() != AliasSet::Flag::Any);
    object = MaybeUnwrap(object);
    MOZ_ASSERT_IF(object, object->type() == MIRType::Object);
    return object;
}

// Generic comparing if a load aliases a store using TI information.
MDefinition::AliasType
AliasAnalysisShared::genericMightAlias(const MDefinition* load, const MDefinition* store)
{
    const MDefinition* loadObject = GetObject(load);
    const MDefinition* storeObject = GetObject(store);
    if (!loadObject || !storeObject)
        return MDefinition::AliasType::MayAlias;

    if (!loadObject->resultTypeSet() || !storeObject->resultTypeSet())
        return MDefinition::AliasType::MayAlias;

    if (loadObject->resultTypeSet()->objectsIntersect(storeObject->resultTypeSet()))
        return MDefinition::AliasType::MayAlias;

    return MDefinition::AliasType::NoAlias;
}


} // namespace jit
} // namespace js
