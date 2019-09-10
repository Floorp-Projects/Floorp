//
// Copyright (c) 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AtomicCounterFunctionHLSL: Class for writing implementation of atomic counter functions into HLSL
// output.
//

#include "compiler/translator/AtomicCounterFunctionHLSL.h"

#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/InfoSink.h"
#include "compiler/translator/IntermNode.h"

namespace sh
{

namespace
{
constexpr ImmutableString kAtomicCounter("atomicCounter");
constexpr ImmutableString kAtomicCounterIncrement("atomicCounterIncrement");
constexpr ImmutableString kAtomicCounterDecrement("atomicCounterDecrement");
constexpr ImmutableString kAtomicCounterBaseName("_acbase_");
}  // namespace

ImmutableString AtomicCounterFunctionHLSL::useAtomicCounterFunction(const ImmutableString &name)
{
    // The largest string that will be create created is "_acbase_increment" or "_acbase_decrement"
    ImmutableStringBuilder hlslFunctionNameSB(kAtomicCounterBaseName.length() +
                                              strlen("increment"));
    hlslFunctionNameSB << kAtomicCounterBaseName;

    AtomicCounterFunction atomicMethod;
    if (kAtomicCounter == name)
    {
        atomicMethod = AtomicCounterFunction::LOAD;
        hlslFunctionNameSB << "load";
    }
    else if (kAtomicCounterIncrement == name)
    {
        atomicMethod = AtomicCounterFunction::INCREMENT;
        hlslFunctionNameSB << "increment";
    }
    else if (kAtomicCounterDecrement == name)
    {
        atomicMethod = AtomicCounterFunction::DECREMENT;
        hlslFunctionNameSB << "decrement";
    }
    else
    {
        atomicMethod = AtomicCounterFunction::INVALID;
        UNREACHABLE();
    }

    ImmutableString hlslFunctionName(hlslFunctionNameSB);
    mAtomicCounterFunctions[hlslFunctionName] = atomicMethod;

    return hlslFunctionName;
}

void AtomicCounterFunctionHLSL::atomicCounterFunctionHeader(TInfoSinkBase &out)
{
    for (auto &atomicFunction : mAtomicCounterFunctions)
    {
        out << "uint " << atomicFunction.first
            << "(in RWByteAddressBuffer counter, int address)\n"
               "{\n";
        switch (atomicFunction.second)
        {
            case AtomicCounterFunction::INCREMENT:
            case AtomicCounterFunction::DECREMENT:
                out << "    uint ret;\n"
                       "    counter.InterlockedAdd(address, ";
                if (atomicFunction.second == AtomicCounterFunction::DECREMENT)
                {
                    out << "0u - ";
                }
                out << "1u, ret);\n"
                    << "    return ret;\n";
                break;
            case AtomicCounterFunction::LOAD:
                out << "    return counter.Load(address);\n";
                break;
            default:
                UNREACHABLE();
                break;
        }
        out << "}\n\n";
    }
}

ImmutableString getAtomicCounterNameForBinding(int binding)
{
    std::stringstream counterName;
    counterName << kAtomicCounterBaseName << binding;
    return ImmutableString(counterName.str());
}

}  // namespace sh
