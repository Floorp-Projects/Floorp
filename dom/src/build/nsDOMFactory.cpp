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
#include "nscore.h"
#include "nsIFactory.h"
#include "nsISupports.h"

#include "nsJSEnvironment.h"
#include "nsIScriptGlobalObject.h"
#include "nsDOMCID.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsIDOMNativeObjectRegistry.h"
#include "nsScriptNameSetRegistry.h"
#include "nsIScriptEventListener.h"
#include "jsurl.h"
#include "nsIScriptContext.h"
#include "nsHTMLTagsEnums.h"
#include "nsIDOMAttr.h"
#include "nsIDOMCDATASection.h"		   
#include "nsIDOMComment.h"
#include "nsIDOMDOMImplementation.h"			   
#include "nsIDOMCharacterData.h"
#include "nsIDOMDocument.h"		   
#include "nsIDOMDocumentFragment.h"	   
#include "nsIDOMDocumentType.h"
#include "nsIDOMElement.h"			   
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"		   
#include "nsIDOMProcessingInstruction.h"	   
#include "nsIDOMText.h"
#include "nsIDOMHTMLAnchorElement.h"
#include "nsIDOMHTMLAppletElement.h"	   
#include "nsIDOMHTMLAreaElement.h"
#include "nsIDOMHTMLBRElement.h"		   
#include "nsIDOMHTMLBaseElement.h"
#include "nsIDOMHTMLBaseFontElement.h"	   
#include "nsIDOMHTMLBodyElement.h"
#include "nsIDOMHTMLButtonElement.h"	   
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLDListElement.h"	   
#include "nsIDOMHTMLDirectoryElement.h"	   
#include "nsIDOMHTMLDivElement.h"
#include "nsIDOMHTMLDocument.h"		   
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLFontElement.h"		   
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLFrameElement.h"	   
#include "nsIDOMHTMLFrameSetElement.h"
#include "nsIDOMHTMLHRElement.h"		   
#include "nsIDOMHTMLHeadElement.h"
#include "nsIDOMHTMLHeadingElement.h"	   
#include "nsIDOMHTMLHtmlElement.h"
#include "nsIDOMHTMLIFrameElement.h"	   
#include "nsIDOMHTMLImageElement.h"
#include "nsIDOMHTMLInputElement.h"	   
#include "nsIDOMHTMLIsIndexElement.h"	   
#include "nsIDOMHTMLLIElement.h"
#include "nsIDOMHTMLLabelElement.h"	   
#include "nsIDOMHTMLLegendElement.h"
#include "nsIDOMHTMLLinkElement.h"		   
#include "nsIDOMHTMLMapElement.h"
#include "nsIDOMHTMLMenuElement.h"		   
#include "nsIDOMHTMLMetaElement.h"
#include "nsIDOMHTMLModElement.h"		   
#include "nsIDOMHTMLOListElement.h"
#include "nsIDOMHTMLObjectElement.h"	   
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLOptionElement.h"	   
#include "nsIDOMHTMLParagraphElement.h"
#include "nsIDOMHTMLParamElement.h"	   
#include "nsIDOMHTMLPreElement.h"
#include "nsIDOMHTMLQuoteElement.h"	   
#include "nsIDOMHTMLScriptElement.h"
#include "nsIDOMHTMLSelectElement.h"	   
#include "nsIDOMHTMLStyleElement.h"
#include "nsIDOMHTMLTableCaptionElement.h"	   
#include "nsIDOMHTMLTableCellElement.h"
#include "nsIDOMHTMLTableColElement.h"	   
#include "nsIDOMHTMLTableElement.h"
#include "nsIDOMHTMLTableRowElement.h"	   
#include "nsIDOMHTMLTableSectionElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMHTMLTitleElement.h"	   
#include "nsIDOMHTMLUListElement.h"
#include "nsIDOMNSHTMLDocument.h"		   
#include "nsIDOMNSHTMLFormElement.h"
#include "nsIDOMNavigator.h"
#include "nsIDOMLocation.h"
#include "nsIDOMStyleSheetCollection.h"
#include "nsIDOMCSS2Properties.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsIDOMCSSStyleRule.h"
#include "nsIDOMCSSStyleRuleCollection.h"
#include "nsIDOMRange.h"
#include "nsIDOMSelection.h"
#include "nsIDOMSelectionListener.h"
#include "plhash.h"

static NS_DEFINE_IID(kIDOMNativeObjectRegistry, NS_IDOM_NATIVE_OBJECT_REGISTRY_IID);

class nsDOMNativeObjectRegistry : public nsIDOMNativeObjectRegistry {
public:
  nsDOMNativeObjectRegistry();
  virtual ~nsDOMNativeObjectRegistry();

  NS_DECL_ISUPPORTS

  NS_IMETHOD RegisterFactory(const nsString& aClassName, const nsIID& aCID);
  NS_IMETHOD GetFactoryCID(const nsString& aClassName, nsIID& aCID);

private:
  static PRIntn RemoveStrings(PLHashEntry *he, PRIntn i, void *arg);

  PLHashTable *mFactories;
};

nsDOMNativeObjectRegistry::nsDOMNativeObjectRegistry()
{
  NS_INIT_REFCNT();
  mFactories = nsnull;
}

PRIntn 
nsDOMNativeObjectRegistry::RemoveStrings(PLHashEntry *he, PRIntn i, void *arg)
{
  char *str = (char *)he->key;

  delete [] str;
  return HT_ENUMERATE_REMOVE;
}

nsDOMNativeObjectRegistry::~nsDOMNativeObjectRegistry()
{
  if (nsnull != mFactories) {
    PL_HashTableEnumerateEntries(mFactories, RemoveStrings, nsnull);
    PL_HashTableDestroy(mFactories);
    mFactories = nsnull;
  }
}

NS_IMPL_ISUPPORTS(nsDOMNativeObjectRegistry, kIDOMNativeObjectRegistry);

NS_IMETHODIMP 
nsDOMNativeObjectRegistry::RegisterFactory(const nsString& aClassName, 
                                           const nsIID& aCID)
{
  if (nsnull == mFactories) {
    mFactories = PL_NewHashTable(4, PL_HashString, PL_CompareStrings,
                                 PL_CompareValues, nsnull, nsnull);
  }

  char *name = aClassName.ToNewCString();
  PL_HashTableAdd(mFactories, name, (void *)&aCID);
  
  return NS_OK;
}
 
NS_IMETHODIMP 
nsDOMNativeObjectRegistry::GetFactoryCID(const nsString& aClassName, 
                                         nsIID& aCID)
{
  if (nsnull == mFactories) {
    return NS_ERROR_FAILURE;
  }
  
  char *name = aClassName.ToNewCString();
  nsIID *classId = (nsIID *)PL_HashTableLookup(mFactories, name);
  delete[] name;

  aCID = *classId;

  return NS_OK;
}


//////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIDOMScriptObjectFactory, NS_IDOM_SCRIPT_OBJECT_FACTORY_IID);

class nsDOMScriptObjectFactory : public nsIDOMScriptObjectFactory {
public:  
  nsDOMScriptObjectFactory();
  virtual ~nsDOMScriptObjectFactory();

  NS_DECL_ISUPPORTS

  NS_IMETHOD    NewScriptAttr(nsIScriptContext *aContext, 
                              nsISupports *aAttribute, 
                              nsISupports *aParent, 
                              void** aReturn);

  NS_IMETHOD    NewScriptComment(nsIScriptContext *aContext, 
                                 nsISupports *aComment,
                                 nsISupports *aParent, 
                                 void** aReturn);
  
  NS_IMETHOD    NewScriptDocument(const nsString &aDocType,
                                  nsIScriptContext *aContext, 
                                  nsISupports *aDocFrag, 
                                  nsISupports *aParent, 
                                  void** aReturn);
  
  NS_IMETHOD    NewScriptDocumentFragment(nsIScriptContext *aContext, 
                                          nsISupports *aDocFrag, 
                                          nsISupports *aParent, 
                                          void** aReturn);
  
  NS_IMETHOD    NewScriptDocumentType(nsIScriptContext *aContext, 
                                      nsISupports *aDocType, 
                                      nsISupports *aParent, 
                                      void** aReturn);
  
  NS_IMETHOD    NewScriptDOMImplementation(nsIScriptContext *aContext, 
                                           nsISupports *aDOM, 
                                           nsISupports *aParent, 
                                           void** aReturn);
  
  NS_IMETHOD    NewScriptCharacterData(PRUint16 aNodeType,
                                       nsIScriptContext *aContext, 
                                       nsISupports *aData, 
                                       nsISupports *aParent, 
                                       void** aReturn);
  
  NS_IMETHOD    NewScriptElement(const nsString &aTagName, 
                                 nsIScriptContext *aContext, 
                                 nsISupports *aElement, 
                                 nsISupports *aParent, 
                                 void **aReturn);

  NS_IMETHOD    NewScriptXMLElement(const nsString &aTagName, 
                                    nsIScriptContext *aContext, 
                                    nsISupports *aElement, 
                                    nsISupports *aParent, 
                                    void **aReturn);
  
  NS_IMETHOD    NewScriptHTMLCollection(nsIScriptContext *aContext, 
                                        nsISupports *aCollection, 
                                        nsISupports *aParent, 
                                        void** aReturn);
  
  NS_IMETHOD    NewScriptNamedNodeMap(nsIScriptContext *aContext, 
                                      nsISupports *aNodeMap, 
                                      nsISupports *aParent, 
                                      void** aReturn);

  NS_IMETHOD    NewScriptNodeList(nsIScriptContext *aContext, 
                                  nsISupports *aNodeList, 
                                  nsISupports *aParent, 
                                  void** aReturn);
  
  NS_IMETHOD    NewScriptProcessingInstruction(nsIScriptContext *aContext, 
                                               nsISupports *aPI, 
                                               nsISupports *aParent, 
                                               void** aReturn);
  
};

nsDOMScriptObjectFactory::nsDOMScriptObjectFactory()
{
  NS_INIT_REFCNT();
}

nsDOMScriptObjectFactory::~nsDOMScriptObjectFactory()
{
}

NS_IMPL_ISUPPORTS(nsDOMScriptObjectFactory, kIDOMScriptObjectFactory);

NS_IMETHODIMP    
nsDOMScriptObjectFactory::NewScriptAttr(nsIScriptContext *aContext, 
                                        nsISupports *aAttribute, 
                                        nsISupports *aParent, 
                                        void** aReturn)
{
  return NS_NewScriptAttr(aContext, aAttribute, aParent, aReturn);
}

NS_IMETHODIMP    
nsDOMScriptObjectFactory::NewScriptComment(nsIScriptContext *aContext,
                                           nsISupports *aComment, 
                                           nsISupports *aParent, 
                                           void** aReturn)
{
  return NS_NewScriptComment(aContext, aComment, aParent, aReturn);
}

NS_IMETHODIMP    
nsDOMScriptObjectFactory::NewScriptDocument(const nsString &aDocType,
                                            nsIScriptContext *aContext, 
                                            nsISupports *aDocFrag, 
                                            nsISupports *aParent, 
                                            void** aReturn)
{
  return NS_NewScriptHTMLDocument(aContext, aDocFrag, aParent, aReturn);
}

NS_IMETHODIMP    
nsDOMScriptObjectFactory::NewScriptDocumentFragment(nsIScriptContext *aContext, 
                                                    nsISupports *aDocFrag, 
                                                    nsISupports *aParent, 
                                                    void** aReturn)
{
  return NS_NewScriptDocumentFragment(aContext, aDocFrag, aParent, aReturn);
}

NS_IMETHODIMP    
nsDOMScriptObjectFactory::NewScriptDocumentType(nsIScriptContext *aContext, 
                                                nsISupports *aDocType, 
                                                nsISupports *aParent,
                                                void** aReturn)
{
  return NS_NewScriptDocumentType(aContext, aDocType, aParent, aReturn);  
}

NS_IMETHODIMP    
nsDOMScriptObjectFactory::NewScriptDOMImplementation(nsIScriptContext *aContext, 
                                                     nsISupports *aDOM, 
                                                     nsISupports *aParent, 
                                                     void** aReturn)
{
  return NS_NewScriptDOMImplementation(aContext, aDOM, aParent, aReturn);
}

NS_IMETHODIMP    
nsDOMScriptObjectFactory::NewScriptCharacterData(PRUint16 aNodeType,
                                                 nsIScriptContext *aContext, 
                                                 nsISupports *aData, 
                                                 nsISupports *aParent, 
                                                 void** aReturn)
{
  if (aNodeType == nsIDOMNode::CDATA_SECTION_NODE) {
    return NS_NewScriptCDATASection(aContext, aData, aParent, aReturn);
  }
  else if (aNodeType == nsIDOMNode::COMMENT_NODE) {
    return NS_NewScriptComment(aContext, aData, aParent, aReturn);
  }
  else {
    return NS_NewScriptText(aContext, aData, aParent, aReturn);
  }
}

NS_IMETHODIMP    
nsDOMScriptObjectFactory::NewScriptElement(const nsString &aTagName, 
                                           nsIScriptContext *aContext, 
                                           nsISupports *aElement, 
                                           nsISupports *aParent, 
                                           void **aReturn)
{
  char *str = aTagName.ToNewCString();
  nsDOMHTMLTag tag = NS_DOMTagToEnum(str);

  if (str) {
    delete[] str;
  }

  switch (tag) {
    case DOMHTMLTag_a:
      return NS_NewScriptHTMLAnchorElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_applet:
      return NS_NewScriptHTMLAppletElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_area:
      return NS_NewScriptHTMLAreaElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_br:
      return NS_NewScriptHTMLBRElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_base:
      return NS_NewScriptHTMLBaseElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_basefont:
      return NS_NewScriptHTMLBaseFontElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_body:
      return NS_NewScriptHTMLBodyElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_button:
      return NS_NewScriptHTMLButtonElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_dl:
      return NS_NewScriptHTMLDListElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_dir:
      return NS_NewScriptHTMLDirectoryElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_div:
      return NS_NewScriptHTMLDivElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_font:
      return NS_NewScriptHTMLFontElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_form:
      return NS_NewScriptHTMLFormElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_frame:
      return NS_NewScriptHTMLFrameElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_frameset:
      return NS_NewScriptHTMLFrameSetElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_hr:
      return NS_NewScriptHTMLHRElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_head:
      return NS_NewScriptHTMLHeadElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_h1:
    case DOMHTMLTag_h2:
    case DOMHTMLTag_h3:
    case DOMHTMLTag_h4:
    case DOMHTMLTag_h5:
    case DOMHTMLTag_h6:
      return NS_NewScriptHTMLHeadingElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_html:
      return NS_NewScriptHTMLHtmlElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_iframe:
      return NS_NewScriptHTMLIFrameElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_img:
      return NS_NewScriptHTMLImageElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_input:
      return NS_NewScriptHTMLInputElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_isindex:
      return NS_NewScriptHTMLIsIndexElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_li:
      return NS_NewScriptHTMLLIElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_label:
      return NS_NewScriptHTMLLabelElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_legend:
      return NS_NewScriptHTMLLegendElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_link:
      return NS_NewScriptHTMLLinkElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_map:
      return NS_NewScriptHTMLMapElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_menu:
      return NS_NewScriptHTMLMenuElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_meta:
      return NS_NewScriptHTMLMetaElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_ins:
    case DOMHTMLTag_del:
      return NS_NewScriptHTMLModElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_ol:
      return NS_NewScriptHTMLOListElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_object:
    case DOMHTMLTag_embed:
      // XXX Does embed need a different one?
      return NS_NewScriptHTMLObjectElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_optgroup:
      return NS_NewScriptHTMLOptGroupElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_option:
      return NS_NewScriptHTMLOptionElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_p:
      return NS_NewScriptHTMLParagraphElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_param:
      return NS_NewScriptHTMLParamElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_pre:
      return NS_NewScriptHTMLPreElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_q:
      return NS_NewScriptHTMLQuoteElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_script:
      return NS_NewScriptHTMLScriptElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_select:
      return NS_NewScriptHTMLSelectElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_style:
      return NS_NewScriptHTMLStyleElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_caption:
      return NS_NewScriptHTMLTableCaptionElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_th:
    case DOMHTMLTag_td:
      return NS_NewScriptHTMLTableCellElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_col:
    case DOMHTMLTag_colgroup:
      return NS_NewScriptHTMLTableColElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_table:
      return NS_NewScriptHTMLTableElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_tr:
      return NS_NewScriptHTMLTableRowElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_thead:
    case DOMHTMLTag_tbody:
    case DOMHTMLTag_tfoot:
      return NS_NewScriptHTMLTableSectionElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_textarea:
      return NS_NewScriptHTMLTextAreaElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_title:
      return NS_NewScriptHTMLTitleElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_ul:
      return NS_NewScriptHTMLUListElement(aContext, aElement, aParent, aReturn);
    case DOMHTMLTag_blockquote:
    case DOMHTMLTag_fieldset:
    case DOMHTMLTag_ilayer:
    case DOMHTMLTag_layer:
    case DOMHTMLTag_multicol:
    case DOMHTMLTag_spacer:
    case DOMHTMLTag_wbr:
      // XXX There should be a separate interface for these
    default:
      return NS_NewScriptHTMLElement(aContext, aElement, aParent, aReturn);
  }
}

NS_IMETHODIMP    
nsDOMScriptObjectFactory::NewScriptXMLElement(const nsString &aTagName, 
                                              nsIScriptContext *aContext, 
                                              nsISupports *aElement, 
                                              nsISupports *aParent, 
                                              void **aReturn)
{
  return NS_NewScriptElement(aContext, aElement, aParent, aReturn);
}

NS_IMETHODIMP    
nsDOMScriptObjectFactory::NewScriptHTMLCollection(nsIScriptContext *aContext, 
                                                  nsISupports *aCollection, 
                                                  nsISupports *aParent, 
                                                  void** aReturn)
{
  return NS_NewScriptHTMLCollection(aContext, aCollection, aParent, aReturn);
}

NS_IMETHODIMP    
nsDOMScriptObjectFactory::NewScriptNamedNodeMap(nsIScriptContext *aContext, 
                                                nsISupports *aNodeMap, 
                                                nsISupports *aParent, 
                                                void** aReturn)
{
  return NS_NewScriptNamedNodeMap(aContext, aNodeMap, aParent, aReturn);
}

NS_IMETHODIMP    
nsDOMScriptObjectFactory::NewScriptNodeList(nsIScriptContext *aContext, 
                                            nsISupports *aNodeList, 
                                            nsISupports *aParent, 
                                            void** aReturn)
{
  return NS_NewScriptNodeList(aContext, aNodeList, aParent, aReturn);
}

NS_IMETHODIMP    
nsDOMScriptObjectFactory::NewScriptProcessingInstruction(nsIScriptContext *aContext, 
                                                         nsISupports *aPI, 
                                                         nsISupports *aParent, 
                                                         void** aReturn)
{
  return NS_NewScriptProcessingInstruction(aContext, aPI, aParent, aReturn);
}


//////////////////////////////////////////////////////////////////////


static NS_DEFINE_IID(kCDOMScriptObjectFactory, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);
static NS_DEFINE_IID(kCDOMNativeObjectRegistry, NS_DOM_NATIVE_OBJECT_REGISTRY_CID);
static NS_DEFINE_IID(kCScriptNameSetRegistry, NS_SCRIPT_NAMESET_REGISTRY_CID);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

class nsDOMFactory : public nsIFactory
{   
  public:   
    // nsISupports methods   
    NS_IMETHOD QueryInterface(const nsIID &aIID,    
                              void **aResult);   
    NS_IMETHOD_(nsrefcnt) AddRef(void);   
    NS_IMETHOD_(nsrefcnt) Release(void);   

    // nsIFactory methods   
    NS_IMETHOD CreateInstance(nsISupports *aOuter,   
                              const nsIID &aIID,   
                              void **aResult);   

    NS_IMETHOD LockFactory(PRBool aLock);   

    nsDOMFactory(const nsCID &aClass);   

  protected:
    virtual ~nsDOMFactory();   

  private:   
    nsrefcnt  mRefCnt;   
    nsCID     mClassID;
};   

nsDOMFactory::nsDOMFactory(const nsCID &aClass)   
{   
  mRefCnt = 0;
  mClassID = aClass;
}   

nsDOMFactory::~nsDOMFactory()   
{   
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
}   

nsresult nsDOMFactory::QueryInterface(const nsIID &aIID,   
                                      void **aResult)   
{   
  if (aResult == NULL) {   
    return NS_ERROR_NULL_POINTER;   
  }   

  // Always NULL result, in case of failure   
  *aResult = NULL;   

  if (aIID.Equals(kISupportsIID)) {   
    *aResult = (void *)(nsISupports*)this;   
  } else if (aIID.Equals(kIFactoryIID)) {   
    *aResult = (void *)(nsIFactory*)this;   
  }   

  if (*aResult == NULL) {   
    return NS_NOINTERFACE;   
  }   

  AddRef(); // Increase reference count for caller   
  return NS_OK;   
}   

nsrefcnt nsDOMFactory::AddRef()   
{   
  return ++mRefCnt;   
}   

nsrefcnt nsDOMFactory::Release()   
{   
  if (--mRefCnt == 0) {   
    delete this;   
    return 0; // Don't access mRefCnt after deleting!   
  }   
  return mRefCnt;   
}  

nsresult nsDOMFactory::CreateInstance(nsISupports *aOuter,  
                                      const nsIID &aIID,  
                                      void **aResult)  
{  
  if (aResult == NULL) {  
    return NS_ERROR_NULL_POINTER;  
  }  

  *aResult = NULL;  
  
  nsISupports *inst = nsnull;

  if (mClassID.Equals(kCDOMScriptObjectFactory)) {
    inst = (nsISupports *)new nsDOMScriptObjectFactory();
  }
  else if (mClassID.Equals(kCDOMNativeObjectRegistry)) {
    inst = (nsISupports *)new nsDOMNativeObjectRegistry();
  }
  else if (mClassID.Equals(kCScriptNameSetRegistry)) {
    inst = (nsISupports *)new nsScriptNameSetRegistry();
  }

  if (inst == NULL) {  
    return NS_ERROR_OUT_OF_MEMORY;  
  }  

  nsresult res = inst->QueryInterface(aIID, aResult);

  if (res != NS_OK) {  
    // We didn't get the right interface, so clean up  
    delete inst;  
  }  

  return res;  
}  

nsresult nsDOMFactory::LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}  

// return the proper factory to the caller
#if defined(XP_MAC) && defined(MAC_STATIC)
extern "C" NS_DOM nsresult
NSGetFactory_DOM_DLL(nsISupports* servMgr,
                     const nsCID &aClass, 
                     const char *aClassName,
                     const char *aProgID,
                     nsIFactory **aFactory)
#else
extern "C" NS_DOM nsresult
NSGetFactory(nsISupports* servMgr,
             const nsCID &aClass, 
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
#endif
{
  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = new nsDOMFactory(aClass);

  if (nsnull == aFactory) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return (*aFactory)->QueryInterface(kIFactoryIID, (void**)aFactory);
}


void XXXDomNeverCalled();
void XXXDomNeverCalled()
{
  nsJSContext* jcx = new nsJSContext(0);
  NS_NewScriptGlobalObject(0);
  NS_NewScriptNavigator(0, 0, 0, 0);
  NS_NewScriptLocation(0, 0, 0, 0);
  NS_NewScriptEventListener(0, 0, 0, 0);
  NS_NewJSEventListener(0, 0, 0);
  NS_NewScriptCSS2Properties(0, 0, 0, 0);
  NS_NewScriptCSSStyleSheet(0, 0, 0, 0);
  NS_NewScriptStyleSheetCollection(0, 0, 0, 0);
  NS_NewScriptCSSStyleRule(0, 0, 0, 0);
  NS_NewScriptCSSStyleRuleCollection(0, 0, 0, 0);
  NS_NewScriptRange(0, 0, 0, 0);
  NS_NewScriptSelection(0, 0, 0, 0);
  NS_NewScriptSelectionListener(0, 0, 0, 0);
  NET_InitJavaScriptProtocol();
  NS_InitDocumentClass(nsnull, nsnull);
}
