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
#ifndef nsHTMLDocument_h___
#define nsHTMLDocument_h___

#include "nsDocument.h"
#include "nsMarkupDocument.h"
#include "nsIHTMLDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMNSHTMLDocument.h"
#include "nsIDOMNode.h"
#include "nsIDOMHTMLBodyElement.h"
#include "nsIHTMLContentContainer.h"
#include "plhash.h"
#include "jsapi.h"

class nsContentList;
class nsIContentViewerContainer;
class nsIHTMLStyleSheet;
class nsIHTMLCSSStyleSheet;
class nsIParser;
class BlockText;
class nsICSSLoader;

class nsHTMLDocument : public nsMarkupDocument,
                       public nsIHTMLDocument,
                       public nsIDOMHTMLDocument,
                       public nsIDOMNSHTMLDocument,
                       public nsIHTMLContentContainer
{
public:
  nsHTMLDocument();
  virtual ~nsHTMLDocument();

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  NS_IMETHOD GetContentType(nsString& aContentType) const;

  NS_IMETHOD CreateShell(nsIPresContext* aContext,
                         nsIViewManager* aViewManager,
                         nsIStyleSet* aStyleSet,
                         nsIPresShell** aInstancePtrResult);

  NS_IMETHOD StartDocumentLoad(const char* aCommand,
#ifdef NECKO
                               nsIChannel* aChannel,
                               nsILoadGroup* aLoadGroup,
#else
                               nsIURI *aUrl, 
#endif
                               nsIContentViewerContainer* aContainer,
                               nsIStreamListener **aDocListener);

  NS_IMETHOD EndLoad();

  NS_IMETHOD AddImageMap(nsIDOMHTMLMapElement* aMap);

  NS_IMETHOD RemoveImageMap(nsIDOMHTMLMapElement* aMap);

  NS_IMETHOD GetImageMap(const nsString& aMapName,
                         nsIDOMHTMLMapElement** aResult);

  NS_IMETHOD GetAttributeStyleSheet(nsIHTMLStyleSheet** aStyleSheet);
  NS_IMETHOD GetInlineStyleSheet(nsIHTMLCSSStyleSheet** aStyleSheet);
  NS_IMETHOD GetCSSLoader(nsICSSLoader*& aLoader);

  NS_IMETHOD GetBaseURL(nsIURI*& aURL) const;
  NS_IMETHOD SetBaseURL(const nsString& aURLSpec);
  NS_IMETHOD GetBaseTarget(nsString& aTarget) const;
  NS_IMETHOD SetBaseTarget(const nsString& aTarget);

  NS_IMETHOD SetLastModified(const nsString& aLastModified);

  NS_IMETHOD GetDTDMode(nsDTDMode& aMode);
  NS_IMETHOD SetDTDMode(nsDTDMode aMode);

  NS_IMETHOD SetHeaderData(nsIAtom* aHeaderField, const nsString& aData);

  NS_IMETHOD ContentAppended(nsIContent* aContainer,
                             PRInt32 aNewIndexInContainer);
  NS_IMETHOD ContentInserted(nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentReplaced(nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentRemoved(nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer);

  // nsIDOMDocument interface
  NS_DECL_IDOMDOCUMENT

  // nsIDOMNode interface
  NS_DECL_IDOMNODE

  // nsIDOMHTMLDocument interface
  NS_DECL_IDOMHTMLDOCUMENT
  NS_DECL_IDOMNSHTMLDOCUMENT
  // the following is not part of nsIDOMHTMLDOCUMENT but allows the content sink to add forms
  NS_IMETHOD AddForm(nsIDOMHTMLFormElement* aForm);

  // From nsIScriptObjectOwner interface, implemented by nsDocument
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);

  /**
    * Finds text in content
   */
  NS_IMETHOD FindNext(const nsString &aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound);

protected:
  // Find/Search Method/Data member
  PRBool SearchBlock(BlockText    & aBlockText, 
                     nsString     & aStr,
                     nsIDOMNode * aCurrentBlock);

  PRBool NodeIsBlock(nsIDOMNode * aNode);
  nsIDOMNode * FindBlockParent(nsIDOMNode * aNode, 
                               PRBool aSkipThisContent = PR_FALSE);

  PRBool BuildBlockTraversing(nsIDOMNode   * aParent,
                              BlockText    & aBlockText,
                              nsIDOMNode * aCurrentBlock);

  PRBool BuildBlockFromContent(nsIDOMNode   * aNode,
                              BlockText    & aBlockText,
                              nsIDOMNode * aCurrentBlock);

  PRBool BuildBlockFromStack(nsIDOMNode * aParent,
                             BlockText  & aBlockText,
                             PRInt32      aStackInx,
                             nsIDOMNode * aCurrentBlock);

  // Search/Find Data Member
  nsIDOMNode ** mParentStack;
  nsIDOMNode ** mChildStack;
  PRInt32       mStackInx;

  nsString   * mSearchStr;
  PRInt32      mSearchDirection;

  PRInt32      mLastBlockSearchOffset;
  PRBool       mAdjustToEnd;

  nsIDOMNode * mHoldBlockContent;
  nsIDOMNode * mBodyContent;

  PRBool       mShouldMatchCase;
  PRBool       mIsPreTag;

protected:
  static PRIntn RemoveStrings(PLHashEntry *he, PRIntn i, void *arg);
  void RegisterNamedItems(nsIContent *aContent, PRBool aInForm);
  void UnregisterNamedItems(nsIContent *aContent, PRBool aInForm);
  nsIContent* FindNamedItem(nsIContent *aContent, const nsString& aName,
                            PRBool aInForm);

  void DeleteNamedItems();
  nsIContent *MatchName(nsIContent *aContent, const nsString& aName);

  virtual void InternalAddStyleSheet(nsIStyleSheet* aSheet);
  virtual void InternalInsertStyleSheetAt(nsIStyleSheet* aSheet, PRInt32 aIndex);
  static PRBool MatchLinks(nsIContent *aContent, nsString* aData);
  static PRBool MatchAnchors(nsIContent *aContent, nsString* aData);
  static PRBool MatchLayers(nsIContent *aContent, nsString* aData);
  static PRBool MatchNameAttribute(nsIContent* aContent, nsString* aData);

  nsresult GetSourceDocumentURL(JSContext* cx, nsIURI** sourceURL);

  PRBool GetBodyContent();
  nsresult GetBodyElement(nsIDOMHTMLBodyElement** aBody);

#ifdef NECKO
  virtual nsresult Reset(nsIChannel* aChannel, nsILoadGroup* aLoadGroup);
#else
  virtual nsresult Reset(nsIURI *aURL);
#endif
  nsresult WriteCommon(const nsString& aText,
                       PRBool aNewlineTerminate);
  nsresult ScriptWriteCommon(JSContext *cx, 
                             jsval *argv, 
                             PRUint32 argc,
                             PRBool aNewlineTerminate);
  nsresult OpenCommon(nsIURI* aUrl);

  nsIHTMLStyleSheet*    mAttrStyleSheet;
  nsIHTMLCSSStyleSheet* mStyleAttrStyleSheet;
  nsIURI*     mBaseURL;
  nsString*   mBaseTarget;
  nsString*   mLastModified;
  nsDTDMode mDTDMode;
  nsVoidArray mImageMaps;
  nsICSSLoader* mCSSLoader;

  nsContentList *mImages;
  nsContentList *mApplets;
  nsContentList *mEmbeds;
  nsContentList *mLinks;
  nsContentList *mAnchors;
  nsContentList *mForms;
  nsContentList *mLayers;
  
  PLHashTable *mNamedItems;

  nsIParser *mParser;

  PRUint32 mIsWriting : 1;
  PRUint32 mWriteLevel : 31;
};

#endif /* nsHTMLDocument_h___ */
