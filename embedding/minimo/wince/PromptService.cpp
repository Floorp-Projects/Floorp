#include "MinimoPrivate.h"

class PromptService: public nsIPromptService
{
public:
  PromptService();
  virtual ~PromptService();
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROMPTSERVICE
private:
  nsCOMPtr<nsIWindowWatcher> mWWatch;
  HWND WndForDOMWindow(nsIDOMWindow *aWindow);
};


NS_IMPL_ISUPPORTS1(PromptService, nsIPromptService)
  
  PromptService::PromptService()
{
  mWWatch = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
}

PromptService::~PromptService()
{
}

HWND
PromptService::WndForDOMWindow(nsIDOMWindow *aWindow)
{
  nsCOMPtr<nsIWebBrowserChrome> chrome;
  HWND val = NULL;
  
  if (mWWatch) {
    nsCOMPtr<nsIDOMWindow> fosterParent;
    if (!aWindow) { // it will be a dependent window. try to find a foster parent.
      mWWatch->GetActiveWindow(getter_AddRefs(fosterParent));
      aWindow = fosterParent;
    }
    mWWatch->GetChromeForWindow(aWindow, getter_AddRefs(chrome));
  }
  
  if (chrome) {
    nsCOMPtr<nsIEmbeddingSiteWindow> site(do_QueryInterface(chrome));
    if (site) {
      site->GetSiteWindow(reinterpret_cast<void **>(&val));
    }
  }
  
  return val;
}

NS_IMETHODIMP
PromptService::Alert(nsIDOMWindow *parent, const PRUnichar *dialogTitle, const PRUnichar *text)
{
  HWND wnd = WndForDOMWindow(parent);
  MessageBoxW(wnd, text, dialogTitle, MB_OK | MB_ICONEXCLAMATION);
  return NS_OK;
}

NS_IMETHODIMP 
PromptService::AlertCheck(nsIDOMWindow *parent,
                          const PRUnichar *dialogTitle,
                          const PRUnichar *text,
                          const PRUnichar *checkboxMsg,
                          PRBool *checkValue)
{
  Alert(parent, dialogTitle, text);
  return NS_OK;
}

NS_IMETHODIMP
PromptService::Confirm(nsIDOMWindow *parent,
                       const PRUnichar *dialogTitle,
                       const PRUnichar *text,
                       PRBool *_retval)
{
  HWND wnd = WndForDOMWindow(parent);
  int choice = MessageBoxW(wnd, text, dialogTitle, MB_YESNO | MB_ICONEXCLAMATION);
  *_retval = choice == IDYES ? PR_TRUE : PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
PromptService::ConfirmCheck(nsIDOMWindow *parent,
                            const PRUnichar *dialogTitle,
                            const PRUnichar *text,
                            const PRUnichar *checkboxMsg,
                            PRBool *checkValue,
                            PRBool *_retval)
{
  Confirm(parent, dialogTitle, text, _retval);
  return NS_OK;
}

NS_IMETHODIMP
PromptService::Prompt(nsIDOMWindow *parent,
                      const PRUnichar *dialogTitle,
                      const PRUnichar *text,
                      PRUnichar **value,
                      const PRUnichar *checkboxMsg,
                      PRBool *checkValue,
                      PRBool *_retval)
{
  
  HWND wnd = WndForDOMWindow(parent);
  MessageBoxW(wnd, L"Prompt", dialogTitle, MB_OK | MB_ICONEXCLAMATION);
  return NS_OK;
}

NS_IMETHODIMP
PromptService::PromptUsernameAndPassword(nsIDOMWindow *parent,
                                         const PRUnichar *dialogTitle,
                                         const PRUnichar *text,
                                         PRUnichar **username,
                                         PRUnichar **password,
                                         const PRUnichar *checkboxMsg,
                                         PRBool *checkValue,
                                         PRBool *_retval)
{
  HWND wnd = WndForDOMWindow(parent);
  MessageBoxW(wnd, L"PromptUsernameAndPassword", dialogTitle, MB_OK | MB_ICONEXCLAMATION);
  return NS_OK;
}

NS_IMETHODIMP
PromptService::PromptPassword(nsIDOMWindow *parent,
                              const PRUnichar *dialogTitle,
                              const PRUnichar *text,
                              PRUnichar **password,
                              const PRUnichar *checkboxMsg,
                              PRBool *checkValue,
                              PRBool *_retval)
{
  HWND wnd = WndForDOMWindow(parent);
  MessageBoxW(wnd, L"PromptPassword", dialogTitle, MB_OK | MB_ICONEXCLAMATION);
  return NS_OK;
}

NS_IMETHODIMP
PromptService::Select(nsIDOMWindow *parent,
                      const PRUnichar *dialogTitle,
                      const PRUnichar *text, PRUint32 count,
                      const PRUnichar **selectList,
                      PRInt32 *outSelection,
                      PRBool *_retval)
{
  HWND wnd = WndForDOMWindow(parent);
  MessageBoxW(wnd, L"Select", dialogTitle, MB_OK | MB_ICONEXCLAMATION);
  return NS_OK;
}

NS_IMETHODIMP 
PromptService::ConfirmEx(nsIDOMWindow *parent,
                         const PRUnichar *dialogTitle,
                         const PRUnichar *text,
                         PRUint32 buttonFlags,
                         const PRUnichar *button0Title,
                         const PRUnichar *button1Title,
                         const PRUnichar *button2Title,
                         const PRUnichar *checkMsg,
                         PRBool *checkValue,
                         PRInt32 *buttonPressed)
{
  HWND wnd = WndForDOMWindow(parent);
  MessageBoxW(wnd, L"ConfirmEx", dialogTitle, MB_OK | MB_ICONEXCLAMATION);
  return NS_OK;
}

class PromptServiceFactory : public nsIFactory {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFACTORY
  
  PromptServiceFactory();
  virtual ~PromptServiceFactory();
};

//*****************************************************************************

NS_IMPL_ISUPPORTS1(PromptServiceFactory, nsIFactory)
  
  PromptServiceFactory::PromptServiceFactory() {
}

PromptServiceFactory::~PromptServiceFactory() {
}

NS_IMETHODIMP
PromptServiceFactory::CreateInstance(nsISupports *aOuter, const nsIID & aIID, void **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  
  *aResult = NULL;  
  PromptService *inst = new PromptService;    
  if (!inst)
    return NS_ERROR_OUT_OF_MEMORY;
  
  nsresult rv = inst->QueryInterface(aIID, aResult);
  if (rv != NS_OK) {  
    // We didn't get the right interface, so clean up  
    delete inst;  
  }  
  
  return rv;
}

NS_IMETHODIMP 
PromptServiceFactory::LockFactory(PRBool lock)
{
  return NS_OK;
}

nsresult NS_NewPromptServiceFactory(nsIFactory** aFactory)
{
  NS_ENSURE_ARG_POINTER(aFactory);
  *aFactory = nsnull;
  
  PromptServiceFactory *result = new PromptServiceFactory;
  if (!result)
    return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(result);
  *aFactory = result;
  
  return NS_OK;
}
