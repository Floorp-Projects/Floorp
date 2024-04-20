class nsPIDOMWindowOuter : public mozIDOMWindowProxy {
  protected:
   using Document = mozilla::dom::Document;
 
   explicit nsPIDOMWindowOuter(uint64_t aWindowID);
 
   ~nsPIDOMWindowOuter();
 
   void NotifyResumingDelayedMedia();
 
  public:
   NS_DECLARE_STATIC_IID_ACCESSOR(NS_PIDOMWINDOWOUTER_IID)
 
   NS_IMPL_FROMEVENTTARGET_HELPER_WITH_GETTER(nsPIDOMWindowOuter,
                                              GetAsOuterWindow())
 
   static nsPIDOMWindowOuter* From(mozIDOMWindowProxy* aFrom) {
     return static_cast<nsPIDOMWindowOuter*>(aFrom);
   }
 
   // Given an inner window, return its outer if the inner is the current inner.
   // Otherwise (argument null or not an inner or not current) return null.
   static nsPIDOMWindowOuter* GetFromCurrentInner(nsPIDOMWindowInner* aInner);
 
   // Check whether a document is currently loading
   inline bool IsLoading() const;
   inline bool IsHandlingResizeEvent() const;
 
   nsPIDOMWindowInner* GetCurrentInnerWindow() const { return mInnerWindow; }
 
   nsPIDOMWindowInner* EnsureInnerWindow() {
     // GetDoc forces inner window creation if there isn't one already
     GetDoc();
     return GetCurrentInnerWindow();
   }
 
   bool IsRootOuterWindow() { return mIsRootOuterWindow; }
 
   // Internal getter/setter for the frame element, this version of the
   // getter crosses chrome boundaries whereas the public scriptable
   // one doesn't for security reasons.
   mozilla::dom::Element* GetFrameElementInternal() const;
   void SetFrameElementInternal(mozilla::dom::Element* aFrameElement);
 
   bool IsBackground() { return mIsBackground; }
 
   // Audio API
   bool GetAudioMuted() const;
 
   // No longer to delay media from starting for this window.
   void ActivateMediaComponents();
   bool ShouldDelayMediaFromStart() const;
 
   void RefreshMediaElementsVolume();
 
   virtual nsPIDOMWindowOuter* GetPrivateRoot() = 0;
 
   /**
    * |top| gets the root of the window hierarchy.
    *
    * This function does not cross chrome-content boundaries, so if this
    * window's parent is of a different type, |top| will return this window.
    *
    * When script reads the top property, we run GetInProcessScriptableTop,
    * which will not cross an <iframe mozbrowser> boundary.
    *
    * In contrast, C++ calls to GetTop are forwarded to GetRealTop, which
    * ignores <iframe mozbrowser> boundaries.
    */
 
   virtual already_AddRefed<nsPIDOMWindowOuter>
   GetInProcessTop() = 0;  // Outer only
   virtual already_AddRefed<nsPIDOMWindowOuter> GetInProcessParent() = 0;
   virtual nsPIDOMWindowOuter* GetInProcessScriptableTop() = 0;
   virtual nsPIDOMWindowOuter* GetInProcessScriptableParent() = 0;
   virtual already_AddRefed<nsPIWindowRoot> GetTopWindowRoot() = 0;
 
   /**
    * Behaves identically to GetInProcessScriptableParent except that it
    * returns null if GetInProcessScriptableParent would return this window.
    */
   virtual nsPIDOMWindowOuter* GetInProcessScriptableParentOrNull() = 0;
 
   virtual void SetIsBackground(bool aIsBackground) = 0;
 
   mozilla::dom::EventTarget* GetChromeEventHandler() const {
     return mChromeEventHandler;
   }
 
   virtual void SetChromeEventHandler(
       mozilla::dom::EventTarget* aChromeEventHandler) = 0;
 
   mozilla::dom::EventTarget* GetParentTarget() {
     if (!mParentTarget) {
       UpdateParentTarget();
     }
     return mParentTarget;
   }
 
   mozilla::dom::ContentFrameMessageManager* GetMessageManager() {
     // We maintain our mMessageManager state alongside mParentTarget.
     if (!mParentTarget) {
       UpdateParentTarget();
     }
     return mMessageManager;
   }
 
   Document* GetExtantDoc() const { return mDoc; }
   nsIURI* GetDocumentURI() const;
 
   Document* GetDoc() {
     if (!mDoc) {
       MaybeCreateDoc();
     }
     return mDoc;
   }
 
   // Set the window up with an about:blank document with the given principal and
   // potentially a CSP and a COEP.
   virtual void SetInitialPrincipal(
       nsIPrincipal* aNewWindowPrincipal, nsIContentSecurityPolicy* aCSP,
       const mozilla::Maybe<nsILoadInfo::CrossOriginEmbedderPolicy>& aCoep) = 0;
 
   // Returns an object containing the window's state.  This also suspends
   // all running timeouts in the window.
   virtual already_AddRefed<nsISupports> SaveWindowState() = 0;
 
   // Restore the window state from aState.
   virtual nsresult RestoreWindowState(nsISupports* aState) = 0;
 
   // Determine if the window is suspended or frozen.  Outer windows
   // will forward this call to the inner window for convenience.  If
   // there is no inner window then the outer window is considered
   // suspended and frozen by default.
   virtual bool IsSuspended() const = 0;
   virtual bool IsFrozen() const = 0;
 
   // Fire any DOM notification events related to things that happened while
   // the window was frozen.
   virtual nsresult FireDelayedDOMEvents(bool aIncludeSubWindows) = 0;
 
   /**
    * Get the docshell in this window.
    */
   inline nsIDocShell* GetDocShell() const;
 
   /**
    * Get the browsing context in this window.
    */
   inline mozilla::dom::BrowsingContext* GetBrowsingContext() const;
 
   /**
    * Get the browsing context group this window belongs to.
    */
   mozilla::dom::BrowsingContextGroup* GetBrowsingContextGroup() const;
 
   /**
    * Set a new document in the window. Calling this method will in most cases
    * create a new inner window. This may be called with a pointer to the current
    * document, in that case the document remains unchanged, but a new inner
    * window will be created.
    *
    * aDocument must not be null.
    */
   virtual nsresult SetNewDocument(
       Document* aDocument, nsISupports* aState, bool aForceReuseInnerWindow,
       mozilla::dom::WindowGlobalChild* aActor = nullptr) = 0;
 
   /**
    * Ensure the size and position of this window are up-to-date by doing
    * a layout flush in the parent (which will in turn, do a layout flush
    * in its parent, etc.).
    */
   virtual void EnsureSizeAndPositionUpToDate() = 0;
 
   /**
    * Suppresses/unsuppresses user initiated event handling in window's document
    * and all in-process descendant documents.
    */
   virtual void SuppressEventHandling() = 0;
   virtual void UnsuppressEventHandling() = 0;
 
   /**
    * Callback for notifying a window about a modal dialog being
    * opened/closed with the window as a parent.
    *
    * If any script can run between the enter and leave modal states, and the
    * window isn't top, the LeaveModalState() should be called on the window
    * returned by EnterModalState().
    */
   virtual nsPIDOMWindowOuter* EnterModalState() = 0;
   virtual void LeaveModalState() = 0;
 
   virtual bool CanClose() = 0;
   virtual void ForceClose() = 0;
 
   /**
    * Moves the top-level window into fullscreen mode if aIsFullScreen is true,
    * otherwise exits fullscreen.
    */
   virtual nsresult SetFullscreenInternal(FullscreenReason aReason,
                                          bool aIsFullscreen) = 0;
   virtual void FullscreenWillChange(bool aIsFullscreen) = 0;
   /**
    * This function should be called when the fullscreen state is flipped.
    * If no widget is involved the fullscreen change, this method is called
    * by SetFullscreenInternal, otherwise, it is called when the widget
    * finishes its change to or from fullscreen.
    *
    * @param aIsFullscreen indicates whether the widget is in fullscreen.
    */
   virtual void FinishFullscreenChange(bool aIsFullscreen) = 0;
 
   virtual void ForceFullScreenInWidget() = 0;
 
   virtual void MacFullscreenMenubarOverlapChanged(
       mozilla::DesktopCoord aOverlapAmount) = 0;
 
   // XXX: These focus methods all forward to the inner, could we change
   // consumers to call these on the inner directly?
 
   /*
    * Get and set the currently focused element within the document. If
    * aNeedsFocus is true, then set mNeedsFocus to true to indicate that a
    * document focus event is needed.
    *
    * DO NOT CALL EITHER OF THESE METHODS DIRECTLY. USE THE FOCUS MANAGER
    * INSTEAD.
    */
   inline mozilla::dom::Element* GetFocusedElement() const;
 
   virtual void SetFocusedElement(mozilla::dom::Element* aElement,
                                  uint32_t aFocusMethod = 0,
                                  bool aNeedsFocus = false) = 0;
   /**
    * Get whether a focused element focused by unknown reasons (like script
    * focus) should match the :focus-visible pseudo-class.
    */
   bool UnknownFocusMethodShouldShowOutline() const;
 
   /**
    * Retrieves the method that was used to focus the current node.
    */
   virtual uint32_t GetFocusMethod() = 0;
 
   /*
    * Tells the window that it now has focus or has lost focus, based on the
    * state of aFocus. If this method returns true, then the document loaded
    * in the window has never received a focus event and expects to receive
    * one. If false is returned, the document has received a focus event before
    * and should only receive one if the window is being focused.
    *
    * aFocusMethod may be set to one of the focus method constants in
    * nsIFocusManager to indicate how focus was set.
    */
   virtual bool TakeFocus(bool aFocus, uint32_t aFocusMethod) = 0;
 
   /**
    * Indicates that the window may now accept a document focus event. This
    * should be called once a document has been loaded into the window.
    */
   virtual void SetReadyForFocus() = 0;
 
   /**
    * Whether the focused content within the window should show a focus ring.
    */
   virtual bool ShouldShowFocusRing() = 0;
 
   /**
    * Indicates that the page in the window has been hidden. This is used to
    * reset the focus state.
    */
   virtual void PageHidden() = 0;
 
   /**
    * Return the window id of this window
    */
   uint64_t WindowID() const { return mWindowID; }
 
   /**
    * Dispatch a custom event with name aEventName targeted at this window.
    * Returns whether the default action should be performed.
    *
    * Outer windows only.
    */
   virtual bool DispatchCustomEvent(
       const nsAString& aEventName,
       mozilla::ChromeOnlyDispatch aChromeOnlyDispatch =
           mozilla::ChromeOnlyDispatch::eNo) = 0;
 
   /**
    * Like nsIDOMWindow::Open, except that we don't navigate to the given URL.
    *
    * Outer windows only.
    */
   virtual nsresult OpenNoNavigate(const nsAString& aUrl, const nsAString& aName,
                                   const nsAString& aOptions,
                                   mozilla::dom::BrowsingContext** _retval) = 0;
 
   /**
    * Fire a popup blocked event on the document.
    */
   virtual void FirePopupBlockedEvent(Document* aDoc, nsIURI* aPopupURI,
                                      const nsAString& aPopupWindowName,
                                      const nsAString& aPopupWindowFeatures) = 0;
 
   // WebIDL-ish APIs
   void MarkUncollectableForCCGeneration(uint32_t aGeneration) {
     mMarkedCCGeneration = aGeneration;
   }
 
   uint32_t GetMarkedCCGeneration() { return mMarkedCCGeneration; }
 
   // XXX(nika): These feel like they should be inner window only, but they're
   // called on the outer window.
   virtual mozilla::dom::Navigator* GetNavigator() = 0;
   virtual mozilla::dom::Location* GetLocation() = 0;
 
   virtual nsresult GetPrompter(nsIPrompt** aPrompt) = 0;
   virtual nsresult GetControllers(nsIControllers** aControllers) = 0;
   virtual already_AddRefed<mozilla::dom::Selection> GetSelection() = 0;
   virtual mozilla::dom::Nullable<mozilla::dom::WindowProxyHolder>
   GetOpener() = 0;
 
   // aLoadState will be passed on through to the windowwatcher.
   // aForceNoOpener will act just like a "noopener" feature in aOptions except
   //                will not affect any other window features.
   virtual nsresult Open(const nsAString& aUrl, const nsAString& aName,
                         const nsAString& aOptions,
                         nsDocShellLoadState* aLoadState, bool aForceNoOpener,
                         mozilla::dom::BrowsingContext** _retval) = 0;
   virtual nsresult OpenDialog(const nsAString& aUrl, const nsAString& aName,
                               const nsAString& aOptions,
                               nsISupports* aExtraArgument,
                               mozilla::dom::BrowsingContext** _retval) = 0;
 
   virtual nsresult GetInnerWidth(double* aWidth) = 0;
   virtual nsresult GetInnerHeight(double* aHeight) = 0;
 
   virtual mozilla::dom::Element* GetFrameElement() = 0;
 
   virtual bool Closed() = 0;
   virtual bool GetFullScreen() = 0;
   virtual nsresult SetFullScreen(bool aFullscreen) = 0;
 
   virtual nsresult Focus(mozilla::dom::CallerType aCallerType) = 0;
   virtual nsresult Close() = 0;
 
   virtual nsresult MoveBy(int32_t aXDif, int32_t aYDif) = 0;
 
   virtual void UpdateCommands(const nsAString& anAction) = 0;
 
   mozilla::dom::DocGroup* GetDocGroup() const;
 
   already_AddRefed<nsIDocShellTreeOwner> GetTreeOwner();
   already_AddRefed<nsIBaseWindow> GetTreeOwnerWindow();
   already_AddRefed<nsIWebBrowserChrome> GetWebBrowserChrome();
 
  protected:
   // Lazily instantiate an about:blank document if necessary, and if
   // we have what it takes to do so.
   void MaybeCreateDoc();
 
   void SetChromeEventHandlerInternal(
       mozilla::dom::EventTarget* aChromeEventHandler);
 
   virtual void UpdateParentTarget() = 0;
 
   // These two variables are special in that they're set to the same
   // value on both the outer window and the current inner window. Make
   // sure you keep them in sync!
   nsCOMPtr<mozilla::dom::EventTarget> mChromeEventHandler;  // strong
   RefPtr<Document> mDoc;
   // Cache the URI when mDoc is cleared.
   nsCOMPtr<nsIURI> mDocumentURI;  // strong
 
   nsCOMPtr<mozilla::dom::EventTarget> mParentTarget;                 // strong
   RefPtr<mozilla::dom::ContentFrameMessageManager> mMessageManager;  // strong
 
   nsCOMPtr<mozilla::dom::Element> mFrameElement;
 
   // These references are used by nsGlobalWindow.
   nsCOMPtr<nsIDocShell> mDocShell;
   RefPtr<mozilla::dom::BrowsingContext> mBrowsingContext;
 
   uint32_t mModalStateDepth;
 
   uint32_t mSuppressEventHandlingDepth;
 
   // Tracks whether our docshell is active.  If it is, mIsBackground
   // is false.  Too bad we have so many different concepts of
   // "active".
   bool mIsBackground;
 
   bool mIsRootOuterWindow;
 
   // And these are the references between inner and outer windows.
   nsPIDOMWindowInner* MOZ_NON_OWNING_REF mInnerWindow;
 
   // A unique (as long as our 64-bit counter doesn't roll over) id for
   // this window.
   uint64_t mWindowID;
 
   uint32_t mMarkedCCGeneration;
 };
 