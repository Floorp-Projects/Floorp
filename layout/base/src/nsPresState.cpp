#include "nsCOMPtr.h"
#include "nsIPresState.h"
#include "nsHashtable.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"
#include "nsXPIDLString.h"

// Static IIDs/CIDs. Try to minimize these.
// None


class nsPresState: public nsIPresState
{
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetStatePropertyAsSupports(const nsAReadableString& aName, nsISupports** aResult);
  NS_IMETHOD SetStatePropertyAsSupports(const nsAReadableString& aName, nsISupports* aValue);

  NS_IMETHOD GetStateProperty(const nsAReadableString& aProperty, nsAWritableString& aResult);
  NS_IMETHOD SetStateProperty(const nsAReadableString& aProperty, const nsAReadableString& aValue);

  NS_IMETHOD RemoveStateProperty(const nsAReadableString& aProperty);

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
nsPresState::GetStateProperty(const nsAReadableString& aName,
			      nsAWritableString& aResult)
{
  // Retrieve from hashtable.
  nsCOMPtr<nsISupportsString> str;
  nsAutoString keyStr(aName);
  nsStringKey key(keyStr);
  if (mPropertyTable)
    str = dont_AddRef(NS_STATIC_CAST(nsISupportsString*, mPropertyTable->Get(&key)));
   
  aResult.SetLength(0);
  if (str) {
    nsXPIDLCString data;
    str->GetData(getter_Copies(data));

    aResult.Append(NS_ConvertUTF8toUCS2(data));
  }
  return NS_OK;
}

NS_IMETHODIMP
nsPresState::SetStateProperty(const nsAReadableString& aName, const nsAReadableString& aValue)
{
  if (!mPropertyTable) {
    mPropertyTable = new nsSupportsHashtable(8);
    NS_ENSURE_TRUE(mPropertyTable, NS_ERROR_OUT_OF_MEMORY);
  }

  // Add to hashtable
  nsAutoString keyStr(aName);
  nsStringKey key(keyStr);

  nsCOMPtr<nsISupportsString> supportsStr(do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID));
  NS_ENSURE_TRUE(supportsStr, NS_ERROR_OUT_OF_MEMORY);

  NS_ConvertUCS2toUTF8 string(aValue);
  supportsStr->SetData(string.get());

  mPropertyTable->Put(&key, supportsStr);
  return NS_OK;
}

NS_IMETHODIMP
nsPresState::RemoveStateProperty(const nsAReadableString& aName)
{
  if (!mPropertyTable)
    return NS_OK;

  nsAutoString keyStr(aName);
  nsStringKey key(keyStr);

  mPropertyTable->Remove(&key);
  return NS_OK;
}

NS_IMETHODIMP
nsPresState::GetStatePropertyAsSupports(const nsAReadableString& aName, nsISupports** aResult)
{
  // Retrieve from hashtable.
  nsCOMPtr<nsISupports> supp;
  nsAutoString keyStr(aName);
  nsStringKey key(keyStr);
  if (mPropertyTable)
    supp = dont_AddRef(NS_STATIC_CAST(nsISupports*, mPropertyTable->Get(&key)));

  *aResult = supp;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsPresState::SetStatePropertyAsSupports(const nsAReadableString& aName, nsISupports* aValue)
{
  if (!mPropertyTable) {
    mPropertyTable = new nsSupportsHashtable(8);
    NS_ENSURE_TRUE(mPropertyTable, NS_ERROR_OUT_OF_MEMORY);
  }

  // Add to hashtable
  nsAutoString keyStr(aName);
  nsStringKey key(keyStr);
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

