#include "nsCOMPtr.h"
#include "nsIPresState.h"
#include "nsHashtable.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"

// Static IIDs/CIDs. Try to minimize these.
// None


class nsPresState: public nsIPresState
{
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetStatePropertyAsSupports(const nsString& aName, nsISupports** aResult);
  NS_IMETHOD SetStatePropertyAsSupports(const nsString& aName, nsISupports* aValue);

  NS_IMETHOD GetStateProperty(const nsString& aProperty, nsString& aResult);
  NS_IMETHOD SetStateProperty(const nsString& aProperty, const nsString& aValue);

public:
  nsPresState();
  virtual ~nsPresState();

// Static members

// Internal member functions
protected:
  
// MEMBER VARIABLES
protected:
  // A string table that holds property/value pairs.
  nsSupportsHashtable* mPropertyTable; 
};

// Static initialization


// Implementation /////////////////////////////////////////////////////////////////

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS1(nsPresState, nsIPresState)

// Constructors/Destructors
nsPresState::nsPresState(void)
:mPropertyTable(nsnull)
{
  NS_INIT_REFCNT();
}

nsPresState::~nsPresState(void)
{
  delete mPropertyTable;
}

// nsIPresState Interface ////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsPresState::GetStateProperty(const nsString& aName, nsString& aResult)
{
  // Retrieve from hashtable.
  nsCOMPtr<nsISupportsWString> str;
  nsStringKey key(aName);
  if (mPropertyTable)
    str = dont_AddRef(NS_STATIC_CAST(nsISupportsWString*, mPropertyTable->Get(&key)));
   
  if (str) {
    PRUnichar* data;
    str->GetData(&data);
    aResult = data;
    nsAllocator::Free(data);
  } else {
    aResult.SetLength(0);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPresState::SetStateProperty(const nsString& aName, const nsString& aValue)
{
  if (!mPropertyTable)
    mPropertyTable = new nsSupportsHashtable(8);

  // Add to hashtable
  nsStringKey key(aName);

  nsCOMPtr<nsISupportsWString> supportsStr;
  nsresult rv = nsComponentManager::CreateInstance(NS_SUPPORTS_WSTRING_PROGID, nsnull, 
                                                    NS_GET_IID(nsISupportsWString), getter_AddRefs(supportsStr));

  PRUnichar* val = aValue.ToNewUnicode();
  supportsStr->SetData(val);
  nsAllocator::Free(val);
  mPropertyTable->Put(&key, supportsStr);
  return NS_OK;
}

NS_IMETHODIMP
nsPresState::GetStatePropertyAsSupports(const nsString& aName, nsISupports** aResult)
{
  // Retrieve from hashtable.
  nsCOMPtr<nsISupports> supp;
  nsStringKey key(aName);
  if (mPropertyTable)
    supp = dont_AddRef(NS_STATIC_CAST(nsISupports*, mPropertyTable->Get(&key)));

  *aResult = supp;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsPresState::SetStatePropertyAsSupports(const nsString& aName, nsISupports* aValue)
{
  if (!mPropertyTable)
    mPropertyTable = new nsSupportsHashtable(8);

  // Add to hashtable
  nsStringKey key(aName);
  mPropertyTable->Put(&key, aValue);
  return NS_OK;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewPresState(nsIPresState** aResult)
{
  *aResult = new nsPresState;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

