/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/**
 * MODULE NOTES:
 * @update  gess 4/8/98
 * 
 *         
 */

#ifndef NS_OTHERHTMLDTD__
#define NS_OTHERHTMLDTD__

#include "nsHTMLDTD.h"
#include "nsHTMLTokens.h"
#include "nshtmlpars.h"


#define NS_IOtherHTML_DTD_IID      \
  {0x8a5e89c0, 0xd16d,  0x11d1,  \
  {0x80, 0x22, 0x00,    0x60, 0x8, 0x14, 0x98, 0x89}}




class COtherDTD : public nsHTMLDTD {
            
	public:

    NS_DECL_ISUPPORTS


    /** -------------------------------------------------------
     *  
     *  
     *  @update  gess 4/9/98
     *  @param   
     *  @return  
     */ //------------------------------------------------------
						COtherDTD();

    /** -------------------------------------------------------
     *  
     *  
     *  @update  gess 4/9/98
     *  @param   
     *  @return  
     */ //------------------------------------------------------
    virtual ~COtherDTD();

    /** ------------------------------------------------------
     *  This method is called to determine whether or not a tag
     *  of one type can contain a tag of another type.
     *  
     *  @update  gess 3/25/98
     *  @param   aParent -- tag enum of parent container
     *  @param   aChild -- tag enum of child container
     *  @return  PR_TRUE if parent can contain child
     */ //----------------------------------------------------
    virtual PRBool CanContain(PRInt32 aParent,PRInt32 aChild) const;

    /** ------------------------------------------------------
     *  This method is called to determine whether or not a tag
     *  of one type can contain a tag of another type.
     *  
     *  @update  gess 3/25/98
     *  @param   aParent -- tag enum of parent container
     *  @param   aChild -- tag enum of child container
     *  @return  PR_TRUE if parent can contain child
     */ //----------------------------------------------------
    virtual PRBool CanContainIndirect(PRInt32 aParent,PRInt32 aChild) const;

    /** -------------------------------------------------------
     *  This method gets called to determine whether a given 
     *  tag can contain newlines. Most do not.
     *  
     *  @update  gess 3/25/98
     *  @param   aTag -- tag to test for containership
     *  @return  PR_TRUE if given tag can contain other tags
     */ //----------------------------------------------------
    virtual PRBool CanOmit(PRInt32 aParent,PRInt32 aChild)const;

    /** -------------------------------------------------------
     *  This method gets called to determine whether a given 
     *  tag is itself a container
     *  
     *  @update  gess 3/25/98
     *  @param   aTag -- tag to test for containership
     *  @return  PR_TRUE if given tag can contain other tags
     */ //----------------------------------------------------
    virtual PRBool IsContainer(PRInt32 aTags) const;

    /** ------------------------------------------------------
     * This method does two things: 1st, help construct
     * our own internal model of the content-stack; and
     * 2nd, pass this message on to the sink.
     * @update	gess4/6/98
     * @param   aNode -- next node to be added to model
     * @return  TRUE if ok, FALSE if error
     */ //----------------------------------------------------
    virtual PRInt32 GetDefaultParentTagFor(PRInt32 aTag) const;


    /** ------------------------------------------------------
     * This method gets called at various times by the parser
     * whenever we want to verify a valid context stack. This
     * method also gives us a hook to add debugging metrics.
     *
     * @update	gess4/6/98
     * @param   aStack[] array of ints (tokens)
     * @param   aCount number of elements in given array
     * @return  TRUE if stack is valid, else FALSE
     */ //-----------------------------------------------------
    virtual PRBool VerifyContextStack(eHTMLTags aStack[],PRInt32 aCount) const;

    /** -------------------------------------------------------
     * This method tries to design a context map (without actually
     * changing our parser state) from the parent down to the
     * child. 
     *
     * @update	gess4/6/98
     * @param   aParent -- tag type of parent
     * @param   aChild -- tag type of child
     * @return  Non zero count of intermediate nodes; 
     *          0 if unable to comply
     */ //----------------------------------------------------
    virtual ForwardPropagate(PRInt32 aVector[],PRInt32 aParent,PRInt32 aChild) const;

    /** -------------------------------------------------------
     * This method tries to design a context map (without actually
     * changing our parser state) from the child up to the parent.
     *
     * @update	gess4/6/98
     * @param   aParent -- tag type of parent
     * @param   aChild -- tag type of child
     * @return  Non zero count of intermediate nodes; 
     *          0 if unable to comply
     */ //----------------------------------------------------
    virtual PRInt32 BackwardPropagate(PRInt32 aVector[],PRInt32 aParent,PRInt32 aChild) const;

};


#endif 

