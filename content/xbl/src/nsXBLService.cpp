/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsIXBLService.h"
#include "nsIInputStream.h"
#include "nsINameSpaceManager.h"
#include "nsHashtable.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIChannel.h"
#include "nsXPIDLString.h"
#include "nsIParser.h"
#include "nsParserCIID.h"
#include "nsNetUtil.h"
#include "plstr.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIBindableContent.h"
#include "nsIDocument.h"
#include "nsIXMLContentSink.h"
#include "nsLayoutCID.h"
#include "nsXMLDocument.h"
#include "nsHTMLAtoms.h"
#include "nsSupportsArray.h"
#include "nsITextContent.h"

#include "nsIXBLBinding.h"

// Static IIDs/CIDs. Try to minimize these.
static NS_DEFINE_CID(kNameSpaceManagerCID,        NS_NAMESPACEMANAGER_CID);
static NS_DEFINE_CID(kXMLDocumentCID,             NS_XMLDOCUMENT_CID);
static NS_DEFINE_CID(kParserCID,                  NS_PARSER_IID); // XXX What's up with this???

// nsProxyStream 
// A helper class used for synchronous parsing of URLs.
class nsProxyStream : public nsIInputStream
{
private:
  const char* mBuffer;
  PRUint32    mSize;
  PRUint32    mIndex;

public:
  nsProxyStream(void) : mBuffer(nsnull)
  {
      NS_INIT_REFCNT();
  }

  virtual ~nsProxyStream(void) {
  }

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIBaseStream
  NS_IMETHOD Close(void) {
      return NS_OK;
  }

  // nsIInputStream
  NS_IMETHOD Available(PRUint32 *aLength) {
      *aLength = mSize - mIndex;
      return NS_OK;
  }

  NS_IMETHOD Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount) {
      PRUint32 readCount = 0;
      while (mIndex < mSize && aCount > 0) {
          *aBuf = mBuffer[mIndex];
          aBuf++;
          mIndex++;
          readCount++;
          aCount--;
      }
      *aReadCount = readCount;
      return NS_OK;
  }

  // Implementation
  void SetBuffer(const char* aBuffer, PRUint32 aSize) {
      mBuffer = aBuffer;
      mSize = aSize;
      mIndex = 0;
  }
};

NS_IMPL_ISUPPORTS(nsProxyStream, NS_GET_IID(nsIInputStream));

//////////////////////////////////////////////////////////////////////////////////////////

class nsXBLService: public nsIXBLService
{
  NS_DECL_ISUPPORTS

  // This function loads a particular XBL file and installs all of the bindings
  // onto the element.
  NS_IMETHOD LoadBindings(nsIContent* aContent, const nsString& aURL);

  // For a given element, returns a flat list of all the anonymous children that need
  // frames built.
  NS_IMETHOD GetContentList(nsIContent* aContent, nsISupportsArray** aResult, nsIContent** aChildElement);

  // Gets the object's base class type.  
  NS_IMETHOD GetBaseTag(nsIContent* aContent, nsIAtom** aResult);

public:
  nsXBLService();
  virtual ~nsXBLService();

  // This method loads a binding doc and then builds the specific binding required.
  NS_IMETHOD GetBinding(nsCAutoString& aURLStr, nsIXBLBinding** aResult);

  // This method checks the hashtable and then calls FetchBindingDocument on a miss.
  NS_IMETHOD GetBindingDocument(nsIURL* aURI, nsIDocument** aResult);

  // This method synchronously loads and parses an XBL file.
  NS_IMETHOD FetchBindingDocument(nsIURI* aURI, nsIDocument** aResult);

  // This method walks a binding document and removes any text nodes
  // that contain only whitespace.
  NS_IMETHOD StripWhitespaceNodes(nsIContent* aContent);

protected:
  // This URIkey class is used to hash URLs into an XBL binding
  // cache.
  class nsIURIKey : public nsHashKey {
  protected:
    nsCOMPtr<nsIURI> mKey;

  public:
    nsIURIKey(nsIURI* key) : mKey(key) {}
    ~nsIURIKey(void) {}

    PRUint32 HashValue(void) const {
      nsXPIDLCString spec;
      mKey->GetSpec(getter_Copies(spec));
      return (PRUint32) PL_HashString(spec);
    }

    PRBool Equals(const nsHashKey *aKey) const {
      PRBool eq;
      mKey->Equals( ((nsIURIKey*) aKey)->mKey, &eq );
      return eq;
    }

    nsHashKey *Clone(void) const {
      return new nsIURIKey(mKey);
    }
  };

// MEMBER VARIABLES
protected: 
  static nsSupportsHashtable* mBindingTable; // This is a table of all the bindings files 
                                             // we have loaded
                                             // during this session.
  static nsINameSpaceManager* gNameSpaceManager; // Used to register the XBL namespace
  static PRInt32  kNameSpaceID_XBL;          // Convenient cached XBL namespace.

  static PRUint32 gRefCnt;                   // A count of XBLservice instances.

  // XBL Atoms
  static nsIAtom* kExtendsAtom; 
  static nsIAtom* kHasChildrenAtom;
};


// Implementation /////////////////////////////////////////////////////////////////

// Static member variable initialization
PRUint32 nsXBLService::gRefCnt = 0;
nsSupportsHashtable* nsXBLService::mBindingTable = nsnull;
nsINameSpaceManager* nsXBLService::gNameSpaceManager = nsnull;

nsIAtom* nsXBLService::kExtendsAtom = nsnull;
nsIAtom* nsXBLService::kHasChildrenAtom = nsnull;

PRInt32 nsXBLService::kNameSpaceID_XBL;

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS1(nsXBLService, nsIXBLService)

// Constructors/Destructors
nsXBLService::nsXBLService(void)
{
  NS_INIT_REFCNT();
  gRefCnt++;
  if (gRefCnt == 1) {
    // Create our binding table.
    mBindingTable = new nsSupportsHashtable();

    // Register the XBL namespace.
    nsresult rv = nsComponentManager::CreateInstance(kNameSpaceManagerCID,
                                                     nsnull,
                                                     NS_GET_IID(nsINameSpaceManager),
                                                     (void**) &gNameSpaceManager);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create namespace manager");
    if (NS_FAILED(rv)) return;

    // XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
    static const char kXBLNameSpaceURI[]
        = "http://www.mozilla.org/xbl";

    rv = gNameSpaceManager->RegisterNameSpace(kXBLNameSpaceURI, kNameSpaceID_XBL);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register XBL namespace");
    if (NS_FAILED(rv)) return;

    // Create our atoms
    kExtendsAtom = NS_NewAtom("extends");
    kHasChildrenAtom = NS_NewAtom("haschildren");
  }
}

nsXBLService::~nsXBLService(void)
{
  gRefCnt--;
  if (gRefCnt == 0) {
    delete mBindingTable;
    NS_IF_RELEASE(gNameSpaceManager);
    
    // Release our atoms
    NS_RELEASE(kExtendsAtom);
    NS_RELEASE(kHasChildrenAtom);
  }
}

// This function loads a particular XBL file and installs all of the bindings
// onto the element.
NS_IMETHODIMP
nsXBLService::LoadBindings(nsIContent* aContent, const nsString& aURL) 
{ 
  nsresult rv;

  nsCOMPtr<nsIBindableContent> bindableContent = do_QueryInterface(aContent);
  if (!bindableContent)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIXBLBinding> binding;
  bindableContent->GetBinding(getter_AddRefs(binding));
  if (binding)
    return NS_OK; // The bindings are already loaded. 
                  // XXX Think about how to flush them when styles cause a dynamic change

  nsCAutoString url = aURL;
  if (NS_FAILED(rv = GetBinding(url, getter_AddRefs(binding)))) {
    NS_ERROR("Failed loading an XBL document for content node.");
    return rv;
  }

  // Install the binding on the content node.
  bindableContent->SetBinding(binding);

  // Tell the binding to build the anonymous content.
  binding->GenerateAnonymousContent(aContent);

  // Tell the binding to install event handlers
  binding->InstallEventHandlers(aContent);

  // XXX Methods and properties. How?
  return NS_OK; 
}

// For a given element, returns a flat list of all the anonymous children that need
// frames built.
NS_IMETHODIMP
nsXBLService::GetContentList(nsIContent* aContent, nsISupportsArray** aResult, nsIContent** aParent)
{ 
  // Iterate over all of the bindings one by one and build up an array
  // of anonymous items.
  *aResult = nsnull;
  *aParent = nsnull;
  nsCOMPtr<nsIBindableContent> bindable = do_QueryInterface(aContent);
  if (!bindable)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIXBLBinding> binding;
  bindable->GetBinding(getter_AddRefs(binding));
  while (binding) {
    // Get the anonymous content.
    nsCOMPtr<nsIContent> content;
    binding->GetAnonymousContent(getter_AddRefs(content));
    if (content) {
      PRInt32 childCount;
      content->ChildCount(childCount);
      for (PRInt32 i = 0; i < childCount; i++) {
        nsCOMPtr<nsIContent> anonymousChild;
        content->ChildAt(i, *getter_AddRefs(anonymousChild));
        if (!(*aResult)) 
          NS_NewISupportsArray(aResult); // This call addrefs the array.

        (*aResult)->AppendElement(anonymousChild);
      }

      binding->GetInsertionPoint(aParent);
      return NS_OK;
    }

    nsCOMPtr<nsIXBLBinding> nextBinding;
    binding->GetBaseBinding(getter_AddRefs(nextBinding));
    binding = nextBinding;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXBLService::GetBaseTag(nsIContent* aContent, nsIAtom** aResult)
{
  *aResult = nsnull;
  nsCOMPtr<nsIBindableContent> bindable = do_QueryInterface(aContent);
  if (!bindable)
    return NS_ERROR_FAILURE;

  return bindable->GetBaseTag(aResult);
}

// Internal helper methods ////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsXBLService::GetBinding(nsCAutoString& aURLStr, nsIXBLBinding** aResult)
{
  *aResult = nsnull;

  if (aURLStr.IsEmpty())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIURL> uri;
  nsComponentManager::CreateInstance("component://netscape/network/standard-url",
                                        nsnull,
                                        NS_GET_IID(nsIURL),
                                        getter_AddRefs(uri));
  uri->SetSpec(aURLStr);

  // XXX Obtain the # marker and remove it from the URL.
  nsXPIDLCString ref;
  uri->GetRef(getter_Copies(ref));
  uri->SetRef("");

  nsCOMPtr<nsIDocument> doc;
  GetBindingDocument(uri, getter_AddRefs(doc));
  if (!doc)
    return NS_ERROR_FAILURE;

  // We have a doc. Obtain our specific binding element.
  // Walk the children looking for the binding that matches the ref
  // specified in the URL.
  nsCOMPtr<nsIContent> root = getter_AddRefs(doc->GetRootContent());
  if (!root)
    return NS_ERROR_FAILURE;

  nsAutoString bindingName(ref);

  PRInt32 count;
  root->ChildCount(count);
  for (PRInt32 i = 0; i < count; i++) {
    nsCOMPtr<nsIContent> child;
    root->ChildAt(i, *getter_AddRefs(child));

    nsAutoString value;
    child->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::name, value);
    
    // If no ref is specified just use this.
    if ((bindingName.IsEmpty()) || (bindingName == value)) {
      // Make a new binding
      NS_NewXBLBinding(aResult);

      // Initialize its bound element.
      (*aResult)->SetBindingElement(child);

      // Check for the presence of an extends attribute
      child->GetAttribute(kNameSpaceID_None, kExtendsAtom, value);
      if (!value.IsEmpty()) {
        // See if we are extending a builtin tag.
        nsCOMPtr<nsIAtom> tag;
        (*aResult)->GetBaseTag(getter_AddRefs(tag));
        if (!tag) {
          // We have a base class binding. Load it right now.
          nsCOMPtr<nsIXBLBinding> baseBinding;
          nsCAutoString url = value;
          GetBinding(url, getter_AddRefs(baseBinding));
          if (!baseBinding)
            return NS_OK; // At least we got the derived class binding loaded.
          (*aResult)->SetBaseBinding(baseBinding);
        }
      }

      break;
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsXBLService::GetBindingDocument(nsIURL* aURI, nsIDocument** aResult)
{
  *aResult = nsnull;
  
  // We've got a file.  Check our key binding file cache.
  nsIURIKey key(aURI);
  nsCOMPtr<nsIDocument> document;
  document = dont_AddRef(NS_STATIC_CAST(nsIDocument*, mBindingTable->Get(&key)));

  if (!document) {
    FetchBindingDocument(aURI, getter_AddRefs(document));
    if (document) {
      // Put the key binding doc into our table.
      mBindingTable->Put(&key, document);
    }
    else return NS_ERROR_FAILURE;
  }

  *aResult = document;
  NS_IF_ADDREF(*aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsXBLService::FetchBindingDocument(nsIURI* aURI, nsIDocument** aResult)
{
  // Initialize our out pointer to nsnull
  *aResult = nsnull;

  // Create the XML document
  nsCOMPtr<nsIDocument> doc;
  nsresult rv = nsComponentManager::CreateInstance(kXMLDocumentCID, nsnull,
                                                   NS_GET_IID(nsIDocument),
                                                   getter_AddRefs(doc));

  if (NS_FAILED(rv)) return rv;

  // XXX This is evil, but we're living in layout, so I'm
  // just going to do it.
  nsXMLDocument* xmlDoc = (nsXMLDocument*)(doc.get());

  // Now we have to synchronously load the binding file.
  // Create an XML content sink and a parser. 

  nsCOMPtr<nsIChannel> channel;
  rv = NS_OpenURI(getter_AddRefs(channel), aURI, nsnull);
  if (NS_FAILED(rv)) return rv;

  // Call StartDocumentLoad
  nsCOMPtr<nsIStreamListener> listener;
  if (NS_FAILED(rv = xmlDoc->StartDocumentLoad("view", channel, 
                                               nsnull, nsnull, getter_AddRefs(listener)))) {
    NS_ERROR("Failure to init XBL doc prior to load.");
    return rv;
  }

  // Now do a blocking synchronous parse of the file.
  nsCOMPtr<nsIInputStream> in;
  PRUint32 sourceOffset = 0;
  rv = channel->OpenInputStream(0, -1, getter_AddRefs(in));

  // If we couldn't open the channel, then just return.
  if (NS_FAILED(rv)) return NS_OK;

  NS_ASSERTION(in != nsnull, "no input stream");
  if (! in) return NS_ERROR_FAILURE;

  rv = NS_ERROR_OUT_OF_MEMORY;
  nsProxyStream* proxy = new nsProxyStream();
  if (! proxy)
    return NS_ERROR_FAILURE;

  listener->OnStartRequest(channel, nsnull);
  while (PR_TRUE) {
    char buf[1024];
    PRUint32 readCount;

    if (NS_FAILED(rv = in->Read(buf, sizeof(buf), &readCount)))
        break; // error

    if (readCount == 0)
        break; // eof

    proxy->SetBuffer(buf, readCount);

    rv = listener->OnDataAvailable(channel, nsnull, proxy, sourceOffset, readCount);
    sourceOffset += readCount;
    if (NS_FAILED(rv))
        break;
  }
  listener->OnStopRequest(channel, nsnull, NS_OK, nsnull);

  // don't leak proxy!
  proxy->Close();
  delete proxy;

  // The document is parsed. We now have a prototype document.
  // Everything worked, so we can just hand this back now.
  *aResult = doc;
  NS_IF_ADDREF(*aResult);

  // The XML content sink produces a ridiculous # of content nodes.
  // It generates text nodes even for whitespace.  The following
  // call walks the generated document tree and trims out these
  // nodes.
  nsCOMPtr<nsIContent> root = getter_AddRefs(doc->GetRootContent());
  if (root)
    StripWhitespaceNodes(root);

  return NS_OK;
}

NS_IMETHODIMP 
nsXBLService::StripWhitespaceNodes(nsIContent* aElement)
{
  PRInt32 childCount;
  aElement->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    aElement->ChildAt(i, *getter_AddRefs(child));
    nsCOMPtr<nsITextContent> text = do_QueryInterface(child);
    if (text) {
      nsAutoString result;
      text->CopyText(result);
      result.StripWhitespace();
      if (result.IsEmpty()) {
        // This node contained nothing but whitespace.
        // Remove it from the content model.
        aElement->RemoveChildAt(i, PR_TRUE);
        i--; // Decrement our count, since we just removed this child.
        childCount--; // Also decrement our total count.
      }
    }
    else StripWhitespaceNodes(child);
  }

  return NS_OK;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLService(nsIXBLService** aResult)
{
  *aResult = new nsXBLService;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

