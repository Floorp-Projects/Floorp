/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set et cin sw=2 sts=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A base class implementing nsIObjectLoadingContent for use by
 * various content nodes that want to provide plugin/document/image
 * loading functionality (eg <embed>, <object>, <applet>, etc).
 */

#ifndef NSOBJECTLOADINGCONTENT_H_
#define NSOBJECTLOADINGCONTENT_H_

#include "nsImageLoadingContent.h"
#include "nsIStreamListener.h"
#include "nsFrameLoader.h"
#include "nsIInterfaceRequestor.h"
#include "nsIChannelEventSink.h"
#include "nsIObjectLoadingContent.h"
#include "nsIRunnable.h"
#include "nsIFrame.h"
#include "nsPluginInstanceOwner.h"
#include "nsIThreadInternal.h"

class nsAsyncInstantiateEvent;
class nsStopPluginRunnable;
class AutoNotifier;
class AutoFallback;
class AutoSetInstantiatingToFalse;
class nsObjectFrame;

enum PluginSupportState {
  ePluginUnsupported,  // The plugin is not supported (e.g. not installed)
  ePluginDisabled,     // The plugin has been explicitly disabled by the user
  ePluginBlocklisted,  // The plugin is blocklisted and disabled
  ePluginOutdated,     // The plugin is considered outdated, but not disabled
  ePluginOtherState,   // Something else (e.g. uninitialized or not a plugin)
  ePluginCrashed,
  ePluginClickToPlay   // The plugin is disabled until the user clicks on it
};

/**
 * INVARIANTS OF THIS CLASS
 * - mChannel is non-null between asyncOpen and onStopRequest (NOTE: Only needs
 *   to be valid until onStopRequest is called on mFinalListener, not
 *   necessarily until the channel calls onStopRequest on us)
 * - mChannel corresponds to the channel that gets passed to the
 *   nsIRequestObserver/nsIStreamListener methods
 * - mChannel can be cancelled and ODA calls will stop
 * - mFinalListener is non-null (only) after onStartRequest has been called on
 *   it and before onStopRequest has been called on it
 *   (i.e. calling onStopRequest doesn't violate the nsIRequestObserver
 *   contract)
 * - mFrameLoader is null while this node is not in a document (XXX this
 *   invariant only exists due to nsFrameLoader suckage and needs to go away)
 * - mInstantiating is true while in LoadObject (it may be true in other
 *   cases as well). Only the function that set mInstantiating should trigger
 *   frame construction or notifications like ContentStatesChanged or flushes.
 */
class nsObjectLoadingContent : public nsImageLoadingContent
                             , public nsIStreamListener
                             , public nsIFrameLoaderOwner
                             , public nsIObjectLoadingContent
                             , public nsIInterfaceRequestor
                             , public nsIChannelEventSink
{
  friend class AutoNotifier;
  friend class AutoFallback;
  friend class AutoSetInstantiatingToFalse;
  friend class nsStopPluginRunnable;
  friend class nsAsyncInstantiateEvent;

  public:
    // This enum's values must be the same as the constants on
    // nsIObjectLoadingContent
    enum ObjectType {
      eType_Loading  = TYPE_LOADING,  ///< Type not yet known
      eType_Image    = TYPE_IMAGE,    ///< This content is an image
      eType_Plugin   = TYPE_PLUGIN,   ///< This content is a plugin
      eType_Document = TYPE_DOCUMENT, ///< This is a document type (e.g. HTML)
      eType_Null     = TYPE_NULL      ///< Type can't be handled
    };

    nsObjectLoadingContent();
    virtual ~nsObjectLoadingContent();

    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIFRAMELOADEROWNER
    NS_DECL_NSIOBJECTLOADINGCONTENT
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSICHANNELEVENTSINK

#ifdef HAVE_CPP_AMBIGUITY_RESOLVING_USING
    // Fix gcc compile warnings
    using nsImageLoadingContent::OnStartRequest;
    using nsImageLoadingContent::OnDataAvailable;
    using nsImageLoadingContent::OnStopRequest;
#endif

    ObjectType Type() { return mType; }

    /**
     * Object state. This is a bitmask consisting of a subset of
     * NS_EVENT_STATE_BROKEN, NS_EVENT_STATE_USERDISABLED and
     * NS_EVENT_STATE_SUPPRESSED representing the current state of the object.
     */
    nsEventStates ObjectState() const;

    void SetIsNetworkCreated(bool aNetworkCreated)
    {
      mNetworkCreated = aNetworkCreated;
    }

    // Can flush layout.
    nsresult InstantiatePluginInstance(const char* aMimeType, nsIURI* aURI);

    void NotifyOwnerDocumentActivityChanged();

    bool SrcStreamLoading() { return mSrcStreamLoading; };

  protected:
    /**
     * Load the object from the given URI.
     * @param aURI       The URI to load.
     * @param aNotify If true, nsIDocumentObserver state change notifications
     *                will be sent as needed.
     * @param aTypeHint  MIME Type hint. Overridden by the server unless this
     *                   class has the eOverrideServerType capability.
     * @param aForceLoad If true, the object will be refetched even if the URI
     *                   is the same as the currently-loaded object.
     * @note Prefer the nsIURI-taking version of this function if a URI object
     *       is already available.
     * @see the URI-taking version of this function for a detailed description
     *      of how a plugin will be found.
     */
    nsresult LoadObject(const nsAString& aURI,
                        bool aNotify,
                        const nsCString& aTypeHint = EmptyCString(),
                        bool aForceLoad = false);
    /**
     * Loads the object from the given URI.
     *
     * The URI and type can both be null; if the URI is null a plugin will be
     * instantiated in the hope that there is a <param> with a useful URI
     * somewhere around. Other attributes of |this| QI'd to nsIContent will be
     * inspected. This function attempts hard to find a suitable plugin.
     *
     * The instantiated plugin depends on three values:
     * - The passed-in URI
     * - The passed-in type hint
     * - The classid attribute, if eSupportClassID is among the capabilities
     *   and such an attribute is present..
     *
     * Supported class ID attributes override any other value.
     *
     * If no class ID is present and aForceType is true, the handler given by
     * aTypeHint will be instantiated for this content.
     * If the URI is null or not supported, and a type hint is given, the plugin
     * corresponding to that type is instantiated.
     *
     * Otherwise a request to that URI is made and the type sent by the server
     * is used to find a suitable handler.
     *
     * @param aForceLoad If true, the object will be refetched even if the URI
     *                   is the same as the currently-loaded object.
     */
    nsresult LoadObject(nsIURI* aURI,
                        bool aNotify,
                        const nsCString& aTypeHint = EmptyCString(),
                        bool aForceLoad = false);

    enum Capabilities {
      eSupportImages    = PR_BIT(0), // Images are supported (imgILoader)
      eSupportPlugins   = PR_BIT(1), // Plugins are supported (nsIPluginHost)
      eSupportDocuments = PR_BIT(2), // Documents are supported
                                     // (nsIDocumentLoaderFactory)
                                     // This flag always includes SVG
      eSupportSVG       = PR_BIT(3), // SVG is supported (image/svg+xml)
      eSupportClassID   = PR_BIT(4), // The classid attribute is supported
      eOverrideServerType = PR_BIT(5) // The server-sent MIME type is ignored
                                      // (ignored if no type is specified)
    };

    /**
     * Returns the list of capabilities this content node supports. This is a
     * bitmask consisting of flags from the Capabilities enum.
     *
     * The default implementation supports all types but no classids.
     */
    virtual PRUint32 GetCapabilities() const;

    /**
     * Fall back to rendering the alternative content.
     */
    void Fallback(bool aNotify);

    /**
     * Subclasses must call this function when they are removed from the
     * document.
     *
     * XXX This is a temporary workaround for docshell suckyness
     */
    void RemovedFromDocument();

    static void Traverse(nsObjectLoadingContent *tmp,
                         nsCycleCollectionTraversalCallback &cb);

    void CreateStaticClone(nsObjectLoadingContent* aDest) const;

    void DoStopPlugin(nsPluginInstanceOwner* aInstanceOwner, bool aDelayedStop,
                      bool aForcedReentry = false);

    nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                        nsIContent* aBindingParent,
                        bool aCompileEventHandler);
    void UnbindFromTree(bool aDeep = true,
                        bool aNullParent = true);

  private:

    void NotifyContentObjectWrapper();

    /**
     * Check whether the given request represents a successful load.
     */
    static bool IsSuccessfulRequest(nsIRequest* aRequest);

    /**
     * Check if the given baseURI is contained in the same directory as the
     * aOriginURI (or a child thereof)
     */
    static bool IsFileCodebaseAllowable(nsIURI* aBaseURI, nsIURI* aOriginURI);

    /**
     * Check whether the URI can be handled internally.
     */
    static bool CanHandleURI(nsIURI* aURI);

    /**
     * Checks whether the given type is a supported document type.
     */
    bool IsSupportedDocument(const nsCString& aType);

    /**
     * Unload the currently loaded content. This removes all state related to
     * the displayed content and sets the type to eType_Null.
     * Note: This does not send any notifications.
     */
    void UnloadContent();

    /**
     * Notifies document observes about a new type/state of this object.
     * Triggers frame construction as needed. mType must be set correctly when
     * this method is called. This method is cheap if the type and state didn't
     * actually change.
     *
     * @param aSync If a synchronous frame construction is required. If false,
     *              the construction may either be sync or async.
     * @param aNotify if false, only need to update the state of our element.
     */
    void NotifyStateChanged(ObjectType aOldType, nsEventStates aOldState,
                            bool aSync, bool aNotify);

    /**
     * Fires the "Plugin not found" event. This function doesn't do any checks
     * whether it should be fired, the caller should do that.
     */
    static void FirePluginError(nsIContent* thisContent, PluginSupportState state);

    ObjectType GetTypeOfContent(const nsCString& aMIMEType);

    /**
     * For a classid, returns the MIME type that can be used to instantiate
     * a plugin for this ID.
     *
     * @return NS_ERROR_NOT_AVAILABLE Unsupported class ID.
     */
    nsresult TypeForClassID(const nsAString& aClassID, nsACString& aType);

    /**
     * Gets the frame that's associated with this content node.
     * Does not flush.
     */
    nsObjectFrame* GetExistingFrame();

    /**
     * Handle being blocked by a content policy.  aStatus is the nsresult
     * return value of the Should* call, while aRetval is what it returned in
     * its out parameter.
     */
    void HandleBeingBlockedByContentPolicy(nsresult aStatus,
                                           PRInt16 aRetval);

    /**
     * Get the plugin support state for the given content node and MIME type.
     * This is used for purposes of determining whether to fire PluginNotFound
     * events etc.  aContentType is the MIME type we ended up with.
     *
     * This should only be called if the type of this content is eType_Null.
     */
    PluginSupportState GetPluginSupportState(nsIContent* aContent, const nsCString& aContentType);

    /**
     * If the plugin for aContentType is disabled, return ePluginDisabled.
     * Otherwise (including if there is no plugin for aContentType at all),
     * return ePluginUnsupported.
     *
     * This should only be called if the type of this content is eType_Null.
     */
    PluginSupportState GetPluginDisabledState(const nsCString& aContentType);

    /**
     * When there is no usable plugin available this will send UI events and
     * update the AutoFallback object appropriate to the reason for there being
     * no plugin available.
     */
    void UpdateFallbackState(nsIContent* aContent, AutoFallback& fallback, const nsCString& aTypeHint);

    nsresult IsPluginEnabledForType(const nsCString& aMIMEType);
    bool IsPluginEnabledByExtension(nsIURI* uri, nsCString& mimeType);

    /**
     * The final listener to ship the data to (imagelib, uriloader, etc)
     */
    nsCOMPtr<nsIStreamListener> mFinalListener;

    /**
     * Frame loader, for content documents we load.
     */
    nsRefPtr<nsFrameLoader>     mFrameLoader;

    /**
     * A pending nsAsyncInstantiateEvent (may be null).  This is a weak ref.
     */
    nsIRunnable                *mPendingInstantiateEvent;

    /**
     * The content type of the resource we were last asked to load.
     */
    nsCString                   mContentType;

    /**
     * The channel that's currently being loaded. This is a weak reference.
     * Non-null between asyncOpen and onStopRequest.
     */
    nsIChannel*                 mChannel;

    // The data we were last asked to load
    nsCOMPtr<nsIURI>            mURI;

    /**
     * Type of the currently-loaded content.
     */
    ObjectType                  mType          : 16;

    /**
     * Whether we are about to call instantiate on our frame. If we aren't,
     * SetFrame needs to asynchronously call Instantiate.
     */
    bool                        mInstantiating : 1;
    // Blocking status from content policy
    bool                        mUserDisabled  : 1;
    bool                        mSuppressed    : 1;

    // True when the object is created for an element which the parser has
    // created using NS_FROM_PARSER_NETWORK flag. If the element is modified,
    // it may lose the flag.
    bool                        mNetworkCreated : 1;

    // Used to keep track of whether or not a plugin should be played.
    // This is used for click-to-play plugins.
    bool                        mShouldPlay : 1;

    // Used to keep track of whether or not a plugin has been played.
    // This is used for click-to-play plugins.
    bool                        mActivated : 1;

    // Protects DoStopPlugin from reentry (bug 724781).
    bool                        mIsStopping : 1;

    // Used to track when we might try to instantiate a plugin instance based on
    // a src data stream being delivered to this object. When this is true we don't
    // want plugin instance instantiation code to attempt to load src data again or
    // we'll deliver duplicate streams. Should be cleared when we are not loading
    // src data.
    bool mSrcStreamLoading;

    // A specific state that caused us to fallback
    PluginSupportState          mFallbackReason;

    nsWeakFrame                 mPrintFrame;

    nsRefPtr<nsPluginInstanceOwner> mInstanceOwner;
};

#endif
