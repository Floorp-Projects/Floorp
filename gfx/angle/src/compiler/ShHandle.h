//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef _SHHANDLE_INCLUDED_
#define _SHHANDLE_INCLUDED_

//
// Machine independent part of the compiler private objects
// sent as ShHandle to the driver.
//
// This should not be included by driver code.
//

#include "GLSLANG/ShaderLang.h"

#include "compiler/InfoSink.h"

class TCompiler;
class TLinker;
class TUniformMap;


//
// The base class used to back handles returned to the driver.
//
class TShHandleBase {
public:
    TShHandleBase() { }
    virtual ~TShHandleBase() { }
    virtual TCompiler* getAsCompiler() { return 0; }
    virtual TLinker* getAsLinker() { return 0; }
    virtual TUniformMap* getAsUniformMap() { return 0; }
};

//
// The base class for the machine dependent linker to derive from
// for managing where uniforms live.
//
class TUniformMap : public TShHandleBase {
public:
    TUniformMap() { }
    virtual ~TUniformMap() { }
    virtual TUniformMap* getAsUniformMap() { return this; }
    virtual int getLocation(const char* name) = 0;    
    virtual TInfoSink& getInfoSink() { return infoSink; }
    TInfoSink infoSink;
};
class TIntermNode;

//
// The base class for the machine dependent compiler to derive from
// for managing object code from the compile.
//
class TCompiler : public TShHandleBase {
public:
    TCompiler(EShLanguage l) : language(l), haveValidObjectCode(false) { }
    virtual ~TCompiler() { }
    EShLanguage getLanguage() { return language; }
    virtual TInfoSink& getInfoSink() { return infoSink; }

    virtual bool compile(TIntermNode* root) = 0;

    virtual TCompiler* getAsCompiler() { return this; }
    virtual bool linkable() { return haveValidObjectCode; }

    TInfoSink infoSink;
protected:
    EShLanguage language;
    bool haveValidObjectCode;
};

//
// Link operations are base on a list of compile results...
//
typedef TVector<TCompiler*> TCompilerList;
typedef TVector<TShHandleBase*> THandleList;

//
// The base class for the machine dependent linker to derive from
// to manage the resulting executable.
//

class TLinker : public TShHandleBase {
public:
    TLinker(EShExecutable e) : 
        executable(e), 
        haveReturnableObjectCode(false),
        appAttributeBindings(0),
        fixedAttributeBindings(0),
        excludedAttributes(0),
        excludedCount(0),
        uniformBindings(0) { }
    virtual TLinker* getAsLinker() { return this; }
    virtual ~TLinker() { }
    virtual bool link(TCompilerList&, TUniformMap*) = 0;
    virtual bool link(THandleList&) { return false; }
    virtual void setAppAttributeBindings(const ShBindingTable* t)   { appAttributeBindings = t; }
    virtual void setFixedAttributeBindings(const ShBindingTable* t) { fixedAttributeBindings = t; }
    virtual void getAttributeBindings(ShBindingTable const **t) const = 0;
    virtual void setExcludedAttributes(const int* attributes, int count) { excludedAttributes = attributes; excludedCount = count; }
    virtual ShBindingTable* getUniformBindings() const  { return uniformBindings; }
    virtual const void* getObjectCode() const { return 0; } // a real compiler would be returning object code here
    virtual TInfoSink& getInfoSink() { return infoSink; }
    TInfoSink infoSink;
protected:
    EShExecutable executable;
    bool haveReturnableObjectCode;  // true when objectCode is acceptable to send to driver

    const ShBindingTable* appAttributeBindings;
    const ShBindingTable* fixedAttributeBindings;
    const int* excludedAttributes;
    int excludedCount;
    ShBindingTable* uniformBindings;                // created by the linker    
};

//
// This is the interface between the machine independent code
// and the machine dependent code.
//
// The machine dependent code should derive from the classes
// above. Then Construct*() and Delete*() will create and 
// destroy the machine dependent objects, which contain the
// above machine independent information.
//
TCompiler* ConstructCompiler(EShLanguage, int);

TShHandleBase* ConstructLinker(EShExecutable, int);
void DeleteLinker(TShHandleBase*);

TUniformMap* ConstructUniformMap();
void DeleteCompiler(TCompiler*);

void DeleteUniformMap(TUniformMap*);

#endif // _SHHANDLE_INCLUDED_
