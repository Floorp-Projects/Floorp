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
#include "nsIDocument.h"
#include "nsIXMLContentSink.h"
#include "nsLayoutCID.h"
#include "nsXMLDocument.h"

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

NS_IMPL_ISUPPORTS(nsProxyStream, nsIInputStream::GetIID());

//////////////////////////////////////////////////////////////////////////////////////////

class nsXBLService: public nsIXBLService
{
  NS_DECL_ISUPPORTS

  // This function loads a particular XBL file and installs all of the bindings
  // onto the element.
  NS_IMETHOD LoadBindings(nsIContent* aContent, const nsString& aURL);

  // For a given element, returns a flat list of all the anonymous children that need
  // frames built.
  NS_IMETHOD GetContentList(nsIContent* aContent, nsISupportsArray** aResult);

public:
  nsXBLService();
  virtual ~nsXBLService();

  // This method checks the hashtable and then calls FetchBindingDocument on a miss.
  NS_IMETHOD GetBindingDocument(nsCAutoString& aURLStr, nsIDocument** aResult);

  // This method synchronously loads and parses an XBL file.
  NS_IMETHOD FetchBindingDocument(nsIURI* aURI, nsIDocument** aResult);

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


};


// Implementation /////////////////////////////////////////////////////////////////

// Static member variable initialization
PRUint32 nsXBLService::gRefCnt = 0;
nsSupportsHashtable* nsXBLService::mBindingTable = nsnull;
nsINameSpaceManager* nsXBLService::gNameSpaceManager = nsnull;

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
                                                     nsCOMTypeInfo<nsINameSpaceManager>::GetIID(),
                                                     (void**) &gNameSpaceManager);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create namespace manager");
    if (NS_FAILED(rv)) return;

    // XXX This is sure to change. Copied from mozilla/layout/xul/content/src/nsXULAtoms.cpp
    static const char kXBLNameSpaceURI[]
        = "http://www.mozilla.org/xbl";

    rv = gNameSpaceManager->RegisterNameSpace(kXBLNameSpaceURI, kNameSpaceID_XBL);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to register XBL namespace");
    if (NS_FAILED(rv)) return;
  }
}

nsXBLService::~nsXBLService(void)
{
  gRefCnt--;
  if (gRefCnt == 0) {
    delete mBindingTable;
    NS_IF_RELEASE(gNameSpaceManager);
  }
}

// This function loads a particular XBL file and installs all of the bindings
// onto the element.
NS_IMETHODIMP
nsXBLService::LoadBindings(nsIContent* aContent, const nsString& aURL) 
{ 
  nsresult rv;
  nsCAutoString url = aURL;
  nsCOMPtr<nsIDocument> document;
  if (NS_FAILED(rv = GetBindingDocument(url, getter_AddRefs(document)))) {
    NS_ERROR("Failed loading an XBL document for content node.");
    return rv;
  }

  return NS_OK; 
}

// For a given element, returns a flat list of all the anonymous children that need
// frames built.
NS_IMETHODIMP
nsXBLService::GetContentList(nsIContent* aContent, nsISupportsArray** aResult)
{ 
  // XXX Implement me!
  return NS_OK;
}


// Internal helper methods ////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsXBLService::GetBindingDocument(nsCAutoString& aURLStr, nsIDocument** aResult)
{
  nsCOMPtr<nsIDocument> document;
  if (aURLStr != nsCAutoString("")) {
    nsCOMPtr<nsIURL> uri;
    nsComponentManager::CreateInstance("component://netscape/network/standard-url",
                                          nsnull,
                                          NS_GET_IID(nsIURL),
                                          getter_AddRefs(uri));
    uri->SetSpec(aURLStr);

    // XXX Obtain the # marker and remove it from the URL.

    // We've got a file.  Check our key binding file cache.
    nsIURIKey key(uri);
    document = dont_AddRef(NS_STATIC_CAST(nsIDocument*, mBindingTable->Get(&key)));

    if (!document) {
      FetchBindingDocument(uri, getter_AddRefs(document));
      if (document) {
        // Put the key binding doc into our table.
        mBindingTable->Put(&key, document);
      }
      else return NS_ERROR_FAILURE;
    }
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
                                                   nsIDocument::GetIID(),
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