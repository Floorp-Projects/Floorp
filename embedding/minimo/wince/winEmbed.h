


extern nsresult CreateBrowserWindow(PRUint32 aChromeFlags, nsIWebBrowserChrome *aParent, nsIWebBrowserChrome **aNewWindow);
extern PRUint32 RunEventLoop(PRBool &aRunCondition);
extern HWND GetBrowserFromChrome(nsIWebBrowserChrome *aChrome);
