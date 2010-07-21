//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

//
// The top level algorithms for linking multiple
// shaders together.
//
#include "compiler/Common.h"
#include "compiler/ShHandle.h"

//
// Actual link object, derived from the shader handle base classes.
//
class TGenericLinker : public TLinker {
public:
    TGenericLinker(EShExecutable e, int dOptions) : TLinker(e), debugOptions(dOptions) { }
    bool link(TCompilerList&, TUniformMap*) { return true; }
    void getAttributeBindings(ShBindingTable const **t) const { }
    int debugOptions;
};

//
// The internal view of a uniform/float object exchanged with the driver.
//
class TUniformLinkedMap : public TUniformMap {
public:
    TUniformLinkedMap() { }
    virtual int getLocation(const char* name) { return 0; }
};

TShHandleBase* ConstructLinker(EShExecutable executable, int debugOptions)
{
    return new TGenericLinker(executable, debugOptions);
}

void DeleteLinker(TShHandleBase* linker)
{
    delete linker;
}

TUniformMap* ConstructUniformMap()
{
    return new TUniformLinkedMap();
}

void DeleteUniformMap(TUniformMap* map)
{
    delete map;
}

TShHandleBase* ConstructBindings()
{
    return 0;
}

void DeleteBindingList(TShHandleBase* bindingList)
{
    delete bindingList;
}
