/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is mozilla.org code
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation
 * Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * Contributor(s):
 *  
 *
 */

#ifndef JSDHANDLES_H___
#define JSDHANDLES_H___

#include "jsdIDebuggerService.h"
#include "jsdebug.h"

class jsdThreadState : public jsdIThreadState
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_JSDITHREADSTATE

    /* you'll normally use use FromPtr() instead of directly constructing one */
    jsdThreadState (JSDThreadState *aThreadState) : mThreadState(aThreadState)
    {
        NS_INIT_ISUPPORTS();
    }

    /* XXX keep track of these somehow, so we don't create new wrapper if we've
     * got one already */
    static jsdIThreadState *FromPtr (JSDThreadState *aThreadState)
    {
        jsdIThreadState *rv = new jsdThreadState (aThreadState);
        NS_IF_ADDREF(rv);
        return rv;
    }

  private:
    jsdThreadState(); /* no implementation */
    jsdThreadState(const jsdThreadState&); /* no implementation */
    
    JSDThreadState *mThreadState;
};

class jsdContext : public jsdIContext
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_JSDICONTEXT

    /* you'll normally use use FromPtr() instead of directly constructing one */
    jsdContext (JSDContext *aCx) : mCx(aCx)
    {
        NS_INIT_ISUPPORTS();
        printf ("++++++ jsdContext: %i\n", mRefCnt);
    }

    /* XXX keep track of these somehow, so we don't create new wrapper if we've
     * got one already */
    static jsdIContext *FromPtr (JSDContext *aCx)
    {
        jsdIContext *rv = new jsdContext (aCx);
        NS_IF_ADDREF(rv);
        return rv;
    }

    virtual ~jsdContext() { printf ("------ ~jsdContext: %i\n", mRefCnt); }
  private:            
    jsdContext(); /* no implementation */
    jsdContext(const jsdContext&); /* no implementation */
    
    JSDContext *mCx;
};

class jsdScript : public jsdIScript
{
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_JSDISCRIPT

    /* you'll normally use use FromPtr() instead of directly constructing one */
    jsdScript (JSDContext *aCx, JSDScript *aScript) : mCx(aCx), mScript(aScript)
    {
        NS_INIT_ISUPPORTS();
    }

    /* XXX keep track of these somehow, so we don't create new wrapper if we've
     * got one already */
    static jsdIScript *FromPtr (JSDContext *aCx, JSDScript *aScript)
    {
        jsdIScript *rv = new jsdScript (aCx, aScript);
        NS_IF_ADDREF(rv);
        return rv;
    }

  private:
    jsdScript(); /* no implementation */
    jsdScript (const jsdScript&); /* no implementation */
    
    JSDContext *mCx;
    JSDScript  *mScript;
};

#endif /* JSDHANDLES_H___ */





#if 0
#define DECL_HANDLE(type,type_uc,jsd_type)                                      \
class jsd##type : public jsdI##type                                             \
{                                                                               \
  public:                                                                       \
    NS_DECL_ISUPPORTS;                                                          \
    NS_DECL_JSDIHANDLE;                                                         \
    NS_DECL_JSDI##type_uc;                                                      \
    static jsd##type *FromPtr (jsd_type *aData)                                 \
        { return new jsd##type (aData); }                                       \
  private:                                                                      \
    jsd_type *mData;                                                            \
    jsdHandle (jsd_type *aData) : mData(aData)                                  \
    {                                                                           \
        NS_INIT_ISUPPORTS();                                                    \
    }                                                                           \
}

DECL_HANDLE(Handle, HANDLE, void *);
DECL_HANDLE(Script, SCRIPT, JSDScript);

#undef DECL_HANDLE

#endif

