#include "nsXBLDocumentInfo.h"
#include "nsHashtable.h"
#include "nsIDocument.h"
#include "nsIXBLPrototypeBinding.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIDOMScriptObjectFactory.h"
#include "jsapi.h"
#include "nsIURI.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"
#include "nsIChromeRegistry.h"

static NS_DEFINE_CID(kDOMScriptObjectFactoryCID, NS_DOM_SCRIPT_OBJECT_FACTORY_CID);

// An XBLDocumentInfo object has a special context associated with it which we can use to pre-compile 
// properties and methods of XBL bindings against.
class nsXBLDocGlobalObject : public nsIScriptGlobalObject,
                             public nsIScriptObjectPrincipal
{
public:
  nsXBLDocGlobalObject();

  // nsISupports interface
  NS_DECL_ISUPPORTS
  
  // nsIScriptGlobalObject methods
  NS_IMETHOD SetContext(nsIScriptContext *aContext);
  NS_IMETHOD GetContext(nsIScriptContext **aContext);
  NS_IMETHOD SetNewDocument(nsIDOMDocument *aDocument,
                            PRBool removeEventListeners);
  NS_IMETHOD SetDocShell(nsIDocShell *aDocShell);
  NS_IMETHOD GetDocShell(nsIDocShell **aDocShell);
  NS_IMETHOD SetOpenerWindow(nsIDOMWindowInternal *aOpener);
  NS_IMETHOD SetGlobalObjectOwner(nsIScriptGlobalObjectOwner* aOwner);
  NS_IMETHOD GetGlobalObjectOwner(nsIScriptGlobalObjectOwner** aOwner);
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext, 
                            nsEvent* aEvent, 
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
  NS_IMETHOD_(JSObject *) GetGlobalJSObject();
  NS_IMETHOD OnFinalize(JSObject *aObject);

  // nsIScriptObjectPrincipal methods
  NS_IMETHOD GetPrincipal(nsIPrincipal** aPrincipal);
    
protected:
  virtual ~nsXBLDocGlobalObject();

  nsCOMPtr<nsIScriptContext> mScriptContext;
  JSObject *mJSObject;    // XXX JS language rabies bigotry badness

  nsIScriptGlobalObjectOwner* mGlobalObjectOwner; // weak reference
  static JSClass gSharedGlobalClass;
};

void PR_CALLBACK nsXBLDocGlobalObject_finalize(JSContext *cx, JSObject *obj)
{
  nsISupports *nativeThis = (nsISupports*)JS_GetPrivate(cx, obj);

  nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(nativeThis));

  if (sgo)
      sgo->OnFinalize(obj);

  // The addref was part of JSObject construction
  NS_RELEASE(nativeThis);
}


JSBool PR_CALLBACK nsXBLDocGlobalObject_resolve(JSContext *cx, JSObject *obj, jsval id)
{
  JSBool did_resolve = JS_FALSE;
  return JS_ResolveStandardClass(cx, obj, id, &did_resolve);
}


JSClass nsXBLDocGlobalObject::gSharedGlobalClass = {
    "nsXBLPrototypeScript compilation scope",
    JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, nsXBLDocGlobalObject_resolve,  JS_ConvertStub,
    nsXBLDocGlobalObject_finalize
};

//----------------------------------------------------------------------
//
// nsXBLDocGlobalObject
//

nsXBLDocGlobalObject::nsXBLDocGlobalObject()
    : mJSObject(nsnull),
      mGlobalObjectOwner(nsnull)
{
  NS_INIT_REFCNT();
}


nsXBLDocGlobalObject::~nsXBLDocGlobalObject()
{}


NS_IMPL_ISUPPORTS2(nsXBLDocGlobalObject, nsIScriptGlobalObject, nsIScriptObjectPrincipal)

//----------------------------------------------------------------------
//
// nsIScriptGlobalObject methods
//

NS_IMETHODIMP
nsXBLDocGlobalObject::SetContext(nsIScriptContext *aContext)
{
  mScriptContext = aContext;
  return NS_OK;
}


NS_IMETHODIMP
nsXBLDocGlobalObject::GetContext(nsIScriptContext **aContext)
{
  // This whole fragile mess is predicated on the fact that
  // GetContext() will be called before GetScriptObject() is.
  if (! mScriptContext) {
    nsCOMPtr<nsIDOMScriptObjectFactory> factory = do_GetService(kDOMScriptObjectFactoryCID);
    NS_ENSURE_TRUE(factory, NS_ERROR_FAILURE);

    nsresult rv =  factory->NewScriptContext(nsnull, getter_AddRefs(mScriptContext));
    if (NS_FAILED(rv))
        return rv;

    JSContext *cx = (JSContext *)mScriptContext->GetNativeContext();

    mJSObject = ::JS_NewObject(cx, &gSharedGlobalClass, nsnull, nsnull);
    if (!mJSObject)
        return NS_ERROR_OUT_OF_MEMORY;

    ::JS_SetGlobalObject(cx, mJSObject);

    // Add an owning reference from JS back to us. This'll be
    // released when the JSObject is finalized.
    ::JS_SetPrivate(cx, mJSObject, this);
    NS_ADDREF(this);
  }

  *aContext = mScriptContext;
  NS_IF_ADDREF(*aContext);
  return NS_OK;
}


NS_IMETHODIMP
nsXBLDocGlobalObject::SetNewDocument(nsIDOMDocument *aDocument,
                                     PRBool removeEventListeners)
{
  NS_NOTREACHED("waaah!");
  return NS_ERROR_UNEXPECTED;
}


NS_IMETHODIMP
nsXBLDocGlobalObject::SetDocShell(nsIDocShell *aDocShell)
{
  NS_NOTREACHED("waaah!");
  return NS_ERROR_UNEXPECTED;
}


NS_IMETHODIMP
nsXBLDocGlobalObject::GetDocShell(nsIDocShell **aDocShell)
{
  NS_WARNING("waaah!");
  return NS_ERROR_UNEXPECTED;
}


NS_IMETHODIMP
nsXBLDocGlobalObject::SetOpenerWindow(nsIDOMWindowInternal *aOpener)
{
  NS_NOTREACHED("waaah!");
  return NS_ERROR_UNEXPECTED;
}


NS_IMETHODIMP
nsXBLDocGlobalObject::SetGlobalObjectOwner(nsIScriptGlobalObjectOwner* aOwner)
{
  mGlobalObjectOwner = aOwner; // weak reference
  return NS_OK;
}


NS_IMETHODIMP
nsXBLDocGlobalObject::GetGlobalObjectOwner(nsIScriptGlobalObjectOwner** aOwner)
{
  *aOwner = mGlobalObjectOwner;
  NS_IF_ADDREF(*aOwner);
  return NS_OK;
}


NS_IMETHODIMP
nsXBLDocGlobalObject::HandleDOMEvent(nsIPresContext* aPresContext, 
                                       nsEvent* aEvent, 
                                       nsIDOMEvent** aDOMEvent,
                                       PRUint32 aFlags,
                                       nsEventStatus* aEventStatus)
{
  NS_NOTREACHED("waaah!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP_(JSObject *)
nsXBLDocGlobalObject::GetGlobalJSObject()
{
  // The prototype document has its own special secret script object
  // that can be used to compile scripts and event handlers.

  if (!mScriptContext)
    return nsnull;

  JSContext* cx = NS_REINTERPRET_CAST(JSContext*,
                                      mScriptContext->GetNativeContext());
  if (!cx)
    return nsnull;

  return ::JS_GetGlobalObject(cx);
}

NS_IMETHODIMP
nsXBLDocGlobalObject::OnFinalize(JSObject *aObject)
{
  NS_ASSERTION(aObject == mJSObject, "Wrong object finalized!");

  mJSObject = nsnull;

  return NS_OK;
}

//----------------------------------------------------------------------
//
// nsIScriptObjectPrincipal methods
//

NS_IMETHODIMP
nsXBLDocGlobalObject::GetPrincipal(nsIPrincipal** aPrincipal)
{
  nsresult rv = NS_OK;
  if (!mGlobalObjectOwner) {
    *aPrincipal = nsnull;
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIXBLDocumentInfo> docInfo = do_QueryInterface(mGlobalObjectOwner, &rv);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocument> document;
  rv = docInfo->GetDocument(getter_AddRefs(document));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  return document->GetPrincipal(aPrincipal);
}

static PRBool IsChromeOrResourceURI(nsIURI* aURI)
{
  PRBool isChrome = PR_FALSE;
  PRBool isResource = PR_FALSE;
  if (NS_SUCCEEDED(aURI->SchemeIs("chrome", &isChrome)) && 
      NS_SUCCEEDED(aURI->SchemeIs("resource", &isResource)))
      return (isChrome || isResource);
  return PR_FALSE;
}

/* Implementation file */
NS_IMPL_ISUPPORTS3(nsXBLDocumentInfo, nsIXBLDocumentInfo, nsIScriptGlobalObjectOwner, nsISupportsWeakReference)

static NS_DEFINE_CID(kChromeRegistryCID,          NS_CHROMEREGISTRY_CID);

nsXBLDocumentInfo::nsXBLDocumentInfo(const char* aDocURI, nsIDocument* aDocument)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
  mDocURI = aDocURI;
  mDocument = aDocument;
  mScriptAccess = PR_TRUE;
  mBindingTable = nsnull;

  nsCOMPtr<nsIURI> uri;
  mDocument->GetDocumentURL(getter_AddRefs(uri));
  if (IsChromeOrResourceURI(uri)) {
    // Cache whether or not this chrome XBL can execute scripts.
    nsCOMPtr<nsIChromeRegistry> reg(do_GetService(kChromeRegistryCID));
    if (reg) {
      PRBool allow = PR_TRUE;
      reg->AllowScriptsForSkin(uri, &allow);
      SetScriptAccess(allow);
    }
  }
}

nsXBLDocumentInfo::~nsXBLDocumentInfo()
{
  /* destructor code */
  if (mGlobalObject) {
    mGlobalObject->SetContext(nsnull); // remove circular reference
    mGlobalObject->SetGlobalObjectOwner(nsnull); // just in case
  }
  delete mBindingTable;
}

NS_IMETHODIMP
nsXBLDocumentInfo::GetPrototypeBinding(const nsAReadableCString& aRef, nsIXBLPrototypeBinding** aResult)
{
  *aResult = nsnull;
  if (!mBindingTable)
    return NS_OK;

  const nsPromiseFlatCString& flat = PromiseFlatCString(aRef);
  nsCStringKey key(flat.get());
  *aResult = NS_STATIC_CAST(nsIXBLPrototypeBinding*, mBindingTable->Get(&key)); // Addref happens here.

  return NS_OK;
}

NS_IMETHODIMP
nsXBLDocumentInfo::SetPrototypeBinding(const nsAReadableCString& aRef, nsIXBLPrototypeBinding* aBinding)
{
  if (!mBindingTable)
    mBindingTable = new nsSupportsHashtable();

  const nsPromiseFlatCString& flat = PromiseFlatCString(aRef);
  nsCStringKey key(flat.get());
  mBindingTable->Put(&key, aBinding);

  return NS_OK;
}

//----------------------------------------------------------------------
//
// nsIScriptGlobalObjectOwner methods
//

NS_IMETHODIMP
nsXBLDocumentInfo::GetScriptGlobalObject(nsIScriptGlobalObject** _result)
{
  if (!mGlobalObject) {
    
    mGlobalObject = new nsXBLDocGlobalObject();
    
    if (!mGlobalObject) {
      *_result = nsnull;
      return NS_ERROR_OUT_OF_MEMORY;
    }
   
    mGlobalObject->SetGlobalObjectOwner(this); // does not refcount
  }

  *_result = mGlobalObject;
  NS_ADDREF(*_result);
  return NS_OK;
}

NS_IMETHODIMP
nsXBLDocumentInfo::ReportScriptError(nsIScriptError *errorObject)
{
  if (errorObject == nsnull)
    return NS_ERROR_NULL_POINTER;

  // Get the console service, where we're going to register the error.
  nsCOMPtr<nsIConsoleService> consoleService  (do_GetService("@mozilla.org/consoleservice;1"));

  if (!consoleService)
    return NS_ERROR_NOT_AVAILABLE;
  return consoleService->LogMessage(errorObject);
}

nsresult NS_NewXBLDocumentInfo(nsIDocument* aDocument, nsIXBLDocumentInfo** aResult)
{
  nsCOMPtr<nsIURI> url;
  aDocument->GetDocumentURL(getter_AddRefs(url));
  
  nsXPIDLCString str;
  url->GetSpec(getter_Copies(str));

  *aResult = new nsXBLDocumentInfo((const char*)str, aDocument);
  
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}
