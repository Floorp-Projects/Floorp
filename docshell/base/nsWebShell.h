#ifndef webshell____h
#define webshell____h

#include "nsIWebShellServices.h"
#include "nsIWebShell.h"
#include "nsILinkHandler.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsIClipboardCommands.h"
#include "nsDocShell.h"

typedef enum {
   eCharsetReloadInit,
   eCharsetReloadRequested,
   eCharsetReloadStopOrigional
} eCharsetReloadState;

class nsWebShell : public nsDocShell,
                   public nsIWebShell,
                   public nsIWebShellContainer,
                   public nsIWebShellServices,
                   public nsILinkHandler,
                   public nsIDocumentLoaderObserver,
                   public nsIClipboardCommands
{
public:
  nsWebShell();
  virtual ~nsWebShell();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIDOCUMENTLOADEROBSERVER
  NS_DECL_NSICLIPBOARDCOMMANDS
  NS_DECL_NSIWEBSHELLSERVICES

  NS_IMETHOD SetupNewViewer(nsIContentViewer* aViewer);

  // nsIContentViewerContainer
  NS_IMETHOD Embed(nsIContentViewer* aDocViewer,
                   const char* aCommand,
                   nsISupports* aExtraInfo);

  // nsIWebShell
  NS_IMETHOD SetContainer(nsIWebShellContainer* aContainer);
  NS_IMETHOD GetContainer(nsIWebShellContainer*& aResult);
  NS_IMETHOD GetTopLevelWindow(nsIWebShellContainer** aWebShellWindow);
  NS_IMETHOD GetRootWebShell(nsIWebShell*& aResult);
  /*NS_IMETHOD SetParent(nsIWebShell* aParent);
  NS_IMETHOD GetParent(nsIWebShell*& aParent);*/
  NS_IMETHOD GetReferrer(nsIURI **aReferrer);

  // Document load api's
  NS_IMETHOD GetDocumentLoader(nsIDocumentLoader*& aResult);

/*  NS_IMETHOD LoadURL(const PRUnichar *aURLSpec,
                     const char* aCommand,
                     nsIInputStream* aPostDataStream=nsnull,
                     PRBool aModifyHistory=PR_TRUE,
                     nsLoadFlags aType = nsIChannel::LOAD_NORMAL,
                     nsISupports * aHistoryState=nsnull,
                     const PRUnichar* aReferrer=nsnull,
                     const char * aWindowTarget = nsnull);

  NS_IMETHOD LoadURI(nsIURI * aUri,
                     const char * aCommand,
                     nsIInputStream* aPostDataStream=nsnull,
                     PRBool aModifyHistory=PR_TRUE,
                     nsLoadFlags aType = nsIChannel::LOAD_NORMAL,
                     nsISupports * aHistoryState=nsnull,
                     const PRUnichar* aReferrer=nsnull,
                     const char * aWindowTarget = nsnull);
  */
  void SetReferrer(const PRUnichar* aReferrer);

  // History api's
  NS_IMETHOD GoTo(PRInt32 aHistoryIndex);
  NS_IMETHOD GetHistoryLength(PRInt32& aResult);
  NS_IMETHOD GetHistoryIndex(PRInt32& aResult);
  NS_IMETHOD GetURL(PRInt32 aHistoryIndex, const PRUnichar** aURLResult);

  // nsIWebShellContainer
  NS_IMETHOD SetHistoryState(nsISupports* aLayoutHistoryState);
  NS_IMETHOD FireUnloadEvent(void);

  // nsILinkHandler
  NS_IMETHOD OnLinkClick(nsIContent* aContent,
                         nsLinkVerb aVerb,
                         const PRUnichar* aURLSpec,
                         const PRUnichar* aTargetSpec,
                         nsIInputStream* aPostDataStream = 0,
			 nsIInputStream* aHeadersDataStream = 0);
  NS_IMETHOD OnOverLink(nsIContent* aContent,
                        const PRUnichar* aURLSpec,
                        const PRUnichar* aTargetSpec);
  NS_IMETHOD GetLinkState(const char* aLinkURI, nsLinkState& aState);


  NS_IMETHOD FindNext(const PRUnichar * aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound);

  // nsIBaseWindow 
  NS_IMETHOD SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
    PRInt32 cy, PRBool fRepaint);
  NS_IMETHOD GetPositionAndSize(PRInt32* x, PRInt32* y, 
   PRInt32* cx, PRInt32* cy);
  NS_IMETHOD Create();
  NS_IMETHOD Destroy();

  // nsWebShell
  nsIEventQueue* GetEventQueue(void);
  void HandleLinkClickEvent(nsIContent *aContent,
                            nsLinkVerb aVerb,
                            const PRUnichar* aURLSpec,
                            const PRUnichar* aTargetSpec,
                            nsIInputStream* aPostDataStream = 0,
			    nsIInputStream* aHeadersDataStream = 0);

  static nsEventStatus PR_CALLBACK HandleEvent(nsGUIEvent *aEvent);

  NS_IMETHOD SetURL(const PRUnichar* aURL);

protected:
  void GetRootWebShellEvenIfChrome(nsIWebShell** aResult);
  void InitFrameData();

  nsIEventQueue* mThreadEventQueue;

  nsIWebShellContainer* mContainer;
  nsIDocumentLoader* mDocLoader;

  PRBool mFiredUnloadEvent;

  nsRect   mBounds;

  eCharsetReloadState mCharsetReloadState;

  nsISupports* mHistoryState; // Weak reference.  Session history owns this.

  nsresult FireUnloadForChildren();

  nsresult CreateViewer(nsIChannel* aChannel,
                        const char* aContentType,
                        const char* aCommand,
                        nsIStreamListener** aResult);

#ifdef DETECT_WEBSHELL_LEAKS
private:
  // We're counting the number of |nsWebShells| to help find leaks
  static unsigned long gNumberOfWebShells;
#endif /* DETECT_WEBSHELL_LEAKS */
};

#endif /* webshell____h */

