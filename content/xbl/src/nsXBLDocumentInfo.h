#include "nsCOMPtr.h"
#include "nsIXBLDocumentInfo.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsWeakReference.h"
#include "nsIDocument.h"

class nsIXBLPrototypeBinding;
class nsSupportsHashtable;

class nsXBLDocumentInfo : public nsIXBLDocumentInfo, public nsIScriptGlobalObjectOwner, public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  
  nsXBLDocumentInfo(const char* aDocURI, nsIDocument* aDocument);
  virtual ~nsXBLDocumentInfo();
  
  NS_IMETHOD GetDocument(nsIDocument** aResult) { *aResult = mDocument; NS_IF_ADDREF(*aResult); return NS_OK; };
  
  NS_IMETHOD GetScriptAccess(PRBool* aResult) { *aResult = mScriptAccess; return NS_OK; };
  NS_IMETHOD SetScriptAccess(PRBool aAccess) { mScriptAccess = aAccess; return NS_OK; };

  NS_IMETHOD GetDocumentURI(nsCString& aDocURI) { aDocURI = mDocURI; return NS_OK; };

  NS_IMETHOD GetPrototypeBinding(const nsAReadableCString& aRef, nsIXBLPrototypeBinding** aResult);
  NS_IMETHOD SetPrototypeBinding(const nsAReadableCString& aRef, nsIXBLPrototypeBinding* aBinding);

  // nsIScriptGlobalObjectOwner methods
  NS_DECL_NSISCRIPTGLOBALOBJECTOWNER

private:
  nsCOMPtr<nsIDocument> mDocument;
  nsCString mDocURI;
  PRBool mScriptAccess;
  nsSupportsHashtable* mBindingTable;

  nsCOMPtr<nsIScriptGlobalObject> mGlobalObject;
};
