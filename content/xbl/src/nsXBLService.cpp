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
 
  static PRUint32 gRefCnt;                   // A count of XBLservice instances.

};


// Implementation /////////////////////////////////////////////////////////////////

// Static member variable initialization
PRUint32 nsXBLService::gRefCnt = 0;
nsSupportsHashtable* nsXBLService::mBindingTable = nsnull;

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS1(nsXBLService, nsIXBLService)

// Constructors/Destructors
nsXBLService::nsXBLService(void)
{
  NS_INIT_REFCNT();
  gRefCnt++;
  if (gRefCnt == 1) {
    mBindingTable = new nsSupportsHashtable();
  }
}

nsXBLService::~nsXBLService(void)
{
  gRefCnt--;
  if (gRefCnt == 0)
    delete mBindingTable;
}

// This function loads a particular XBL file and installs all of the bindings
// onto the element.
NS_IMETHODIMP
nsXBLService::LoadBindings(nsIContent* aContent, const nsString& aURL) 
{ 

  return NS_OK; 
}

// For a given element, returns a flat list of all the anonymous children that need
// frames built.
NS_IMETHODIMP
nsXBLService::GetContentList(nsIContent* aContent, nsISupportsArray** aResult)
{ 
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