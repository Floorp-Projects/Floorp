#include "nsCOMPtr.h"
#include "nsIXBLBinding.h"
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

#include "nsIXBLBinding.h"

// Static IIDs/CIDs. Try to minimize these.
// None


class nsXBLBinding: public nsIXBLBinding
{
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetBaseBinding(nsIXBLBinding** aResult);
  NS_IMETHOD SetBaseBinding(nsIXBLBinding* aBinding);

  NS_IMETHOD GetAnonymousContent(nsIContent** aParent);
  NS_IMETHOD SetAnonymousContent(nsIContent* aParent);

  NS_IMETHOD GetBindingElement(nsIContent** aResult);
  NS_IMETHOD SetBindingElement(nsIContent* aElement);

  NS_IMETHOD GenerateAnonymousContent(nsIContent* aBoundElement);
  NS_IMETHOD InstallEventHandlers(nsIContent* aBoundElement);

public:
  nsXBLBinding();
  virtual ~nsXBLBinding();

// MEMBER VARIABLES
protected:
  nsCOMPtr<nsIContent> binding; // Strong. As long as we're around, the binding can't go away.
  nsCOMPtr<nsIContent> content; // Strong. Our anonymous content stays around with us.
  nsCOMPtr<nsIXBLBinding> nextBinding; // Strong. The derived binding owns the base class bindings.
};


// Implementation /////////////////////////////////////////////////////////////////

// Implement our nsISupports methods
NS_IMPL_ISUPPORTS1(nsXBLBinding, nsIXBLBinding)

// Constructors/Destructors
nsXBLBinding::nsXBLBinding(void)
{
  NS_INIT_REFCNT();
}

nsXBLBinding::~nsXBLBinding(void)
{
}

// nsIXBLBinding Interface ////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsXBLBinding::GetBaseBinding(nsIXBLBinding** aResult)
{
  *aResult = nextBinding;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::SetBaseBinding(nsIXBLBinding* aBinding)
{
  nextBinding = aBinding; // Comptr handles rel/add
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GetAnonymousContent(nsIContent** aResult)
{
  *aResult = content;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::SetAnonymousContent(nsIContent* aParent)
{
  content = aParent;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GetBindingElement(nsIContent** aResult)
{
  *aResult = binding;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::SetBindingElement(nsIContent* aElement)
{
  binding = aElement;
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::GenerateAnonymousContent(nsIContent* aBoundElement)
{
  return NS_OK;
}

NS_IMETHODIMP
nsXBLBinding::InstallEventHandlers(nsIContent* aBoundElement)
{
  // XXX Implement me!
  return NS_OK;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewXBLBinding(nsIXBLBinding** aResult)
{
  *aResult = new nsXBLBinding;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}