/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

#ifndef nsIEditor_h__
#define nsIEditor_h__
#include "nsISupports.h"

class nsIDOMElement;
class nsITransaction;

/*
Editor interface to outside world
*/

#define NS_IEDITOR_IID \
{/* A3C5EE71-742E-11d2-8F2C-006008310194*/ \
0xa3c5ee71, 0x742e, 0x11d2, \
{0x8f, 0x2c, 0x0, 0x60, 0x8, 0x31, 0x1, 0x94} }

#define NS_IEDITORFACTORY_IID \
{ /* {E2F4C7F1-864A-11d2-8F38-006008310194}*/ \
0xe2f4c7f1, 0x864a, 0x11d2, \
{ 0x8f, 0x38, 0x0, 0x60, 0x8, 0x31, 0x1, 0x94 } }


class nsIDOMDocument;
class nsIPresShell;
class nsString;

/**
 * A generic editor interface. 
 * <P>
 * nsIEditor is the base interface used by applications to communicate with the editor.  
 * It makes no assumptions about the kind of content it is editing, 
 * other than the content being a DOM tree. 
 */
class nsIEditor  : public nsISupports{
public:

  typedef enum {NONE = 0,BOLD = 1,NUMPROPERTIES} Properties;

  typedef enum {eLTR=0, eRTL=1} Direction;

  static const nsIID& IID() { static nsIID iid = NS_IEDITOR_IID; return iid; }

  /**
   * Init tells is to tell the implementation of nsIEditor to begin its services
   * @param aDomInterface The dom interface being observed
   */
  virtual nsresult Init(nsIDOMDocument *aDomInterface,
                        nsIPresShell   *aPresShell) = 0;

  /**
   * GetDomInterface() WILL NOT RETURN AN ADDREFFED POINTER
   *
   * @param aDomInterface The dom interface being observed
   */
  virtual nsresult GetDomInterface(nsIDOMDocument **)=0;

  /**
   * SetProperties() sets the properties of the current selection
   *
   * @param aProperty An enum that lists the various properties that can be applied, bold, ect.
   */
  virtual nsresult SetProperties(Properties aProperties)=0;

  /**
   * SetProperties() sets the properties of the current selection
   *
   * @param aProperty An enum that will recieve the various properties that can be applied from the current selection.
   */
  virtual nsresult GetProperties(Properties **aProperties)=0;

  /**
   * SetAttribute() sets the attribute of aElement.
   * No checking is done to see if aAttribute is a legal attribute of the node,
   * or if aValue is a legal value of aAttribute.
   *
   * @param aElement    the content element to operate on
   * @param aAttribute  the string representation of the attribute to set
   * @param aValue      the value to set aAttribute to
   */
  virtual nsresult SetAttribute(nsIDOMElement * aElement, 
                                const nsString& aAttribute, 
                                const nsString& aValue)=0;

  /**
   * GetAttributeValue() retrieves the attribute's value for aElement.
   *
   * @param aElement      the content element to operate on
   * @param aAttribute    the string representation of the attribute to get
   * @param aResultValue  the value of aAttribute.  only valid if aResultIsSet is PR_TRUE
   * @param aResultIsSet  PR_TRUE if aAttribute is set on the current node, PR_FALSE if it is not.
   */
  virtual nsresult GetAttributeValue(nsIDOMElement * aElement, 
                                     const nsString& aAttribute, 
                                     nsString&       aResultValue, 
                                     PRBool&         aResultIsSet)=0;

  /**
   * RemoveAttribute() deletes aAttribute from the attribute list of aElement.
   * If aAttribute is not an attribute of aElement, nothing is done.
   *
   * @param aElement      the content element to operate on
   * @param aAttribute    the string representation of the attribute to get
   */
  virtual nsresult RemoveAttribute(nsIDOMElement * aElement, 
                                   const nsString& aAttribute)=0;

  /**
   * InsertString() Inserts a string at the current location
   *
   * @param aString An nsString that is the string to be inserted
   */
  virtual nsresult InsertString(nsString *aString)=0;

  /**
   * Commit(PRBool aCtrlKey) is a catch all method.  It may be depricated later.
   * <BR>It is to respond to RETURN keys and CTRL return keys.  Its action depends
   * on the selection at the time.  It may insert a <BR> or a <P> or activate the properties of the element
   * @param aCtrlKey  was the CtrlKey down?
   */
  virtual nsresult Commit(PRBool aCtrlKey)=0;

  /** Do fires a transaction.  It is provided here so clients can create their own transactions.
    * Clients need no knowledge of whether the editor has a transaction manager or not.
    * If a transaction manager is present, it is used.  
    * Otherwise, the transaction is just executed directly.
    *
    * @param aTxn the transaction to execute
    */
  virtual nsresult Do(nsITransaction *aTxn)=0;

  /** Undo reverses the effects of the last ExecuteTransaction operation
    * It is provided here so clients need no knowledge of whether the editor has a transaction manager or not.
    * If a transaction manager is present, it is told to undo and the result of
    * that undo is returned.  
    * Otherwise, the Undo request is ignored.
    *
    */
  virtual nsresult Undo(PRUint32 aCount)=0;

  /** Redo reverses the effects of the last Undo operation
    * It is provided here so clients need no knowledge of whether the editor has a transaction manager or not.
    * If a transaction manager is present, it is told to redo and the result of
    * that redo is returned.  
    * Otherwise, the Redo request is ignored.
    *
    */
  virtual nsresult Redo(PRUint32 aCount)=0;

  /** BeginUpdate is a signal from the caller to the editor that the caller will execute multiple updates
    * to the content tree that should be treated in the most efficient way possible.
    * EndUpdate must be called after BeginUpdate.
    * Calls to BeginUpdate can be nested, as long as EndUpdate is called once per BeginUpdate.
    */
  virtual nsresult BeginUpdate()=0;

  /** EndUpdate is a signal from the caller to the editor that the caller is finished updating the content model.
    * BeginUpdate must be called before EndUpdate is called.
    * Calls to BeginUpdate can be nested, as long as EndUpdate is called once per BeginUpdate.
    */
  virtual nsresult EndUpdate()=0;


};

#endif //nsIEditor_h__

