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
#include "nsPluginInstanceOwner.h"
#include "nsIThreadInternal.h"
#include "nsIFrame.h"

class nsAsyncInstantiateEvent;
class nsStopPluginRunnable;
class AutoNotifier;
class AutoFallback;
class AutoSetInstantiatingToFalse;
class nsObjectFrame;

class nsObjectLoadingContent : public nsImageLoadingContent
                             , public nsIStreamListener
                             , public nsIFrameLoaderOwner
                             , public nsIObjectLoadingContent
                             , public nsIInterfaceRequestor
                             , public nsIChannelEventSink
{
  friend class AutoSetInstantiatingToFalse;
  friend class AutoSetLoadingToFalse;
  friend class InDocCheckEvent;
  friend class nsStopPluginRunnable;
  friend class nsAsyncInstantiateEvent;

  public:
    // This enum's values must be the same as the constants on
    // nsIObjectLoadingContent
    enum ObjectType {
      // Loading, type not yet known. We may be waiting for a channel to open.
      eType_Loading        = TYPE_LOADING,
      // Content is a *non-svg* image
      eType_Image          = TYPE_IMAGE,
      // Content is a plugin
      eType_Plugin         = TYPE_PLUGIN,
      // Content is a subdocument, possibly SVG
      eType_Document       = TYPE_DOCUMENT,
      // No content loaded (fallback). May be showing alternate content or
      // a custom error handler - *including* click-to-play dialogs
      eType_Null           = TYPE_NULL
    };
    enum FallbackType {
      eFallbackUnsupported,  // The content type is not supported (e.g. plugin 
                             // not installed)
      eFallbackAlternate,    // Showing alternate content
      eFallbackDisabled,     // The plugin exists, but is disabled
      eFallbackBlocklisted,  // The plugin is blocklisted and disabled
      eFallbackOutdated,     // The plugin is considered outdated, but not
                             // disabled
      eFallbackCrashed,      // The plugin has crashed
      eFallbackSuppressed,   // Suppressed by security policy
      eFallbackUserDisabled, // Blocked by content policy
      eFallbackClickToPlay,  // The plugin is disabled until the user clicks on
                             // it
      eFallbackVulnerableUpdatable, // The plugin is vulnerable (update avail)
      eFallbackVulnerableNoUpdate  // The plugin is vulnerable (no update avail)
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

    /**
     * Object state. This is a bitmask of NS_EVENT_STATEs epresenting the
     * current state of the object.
     */
    nsEventStates ObjectState() const;

    ObjectType Type() { return mType; }

    void SetIsNetworkCreated(bool aNetworkCreated)
    {
      mNetworkCreated = aNetworkCreated;
    }

    /**
     * Immediately instantiate a plugin instance. This is a no-op if
     * mType != eType_Plugin or a plugin is already running.
     */
    nsresult InstantiatePluginInstance();

    /**
     * Notify this class the document state has changed
     * Called by nsDocument so we may suspend plugins in inactive documents)
     */
    void NotifyOwnerDocumentActivityChanged();

    /**
     * Used by pluginHost to know if we're loading with a channel, so it
     * will not open its own.
     */
    bool SrcStreamLoading() { return mSrcStreamLoading; };

  protected:
    /**
     * Begins loading the object when called
     *
     * Attributes of |this| QI'd to nsIContent will be inspected, depending on
     * the node type. This function currently assumes it is a <applet>,
     * <object>, or <embed> tag.
     *
     * The instantiated plugin depends on:
     * - The URI (<embed src>, <object data>)
     * - The type 'hint' (type attribute)
     * - The mime type returned by opening the URI
     * - Enabled plugins claiming the ultimate mime type
     * - The capabilities returned by GetCapabilities
     * - The classid attribute, if eSupportClassID is among the capabilities
     *
     * If eAllowPluginSkipChannel is true, we may skip opening the URI if our
     * type hint points to a valid plugin, deferring that responsibility to the
     * plugin.
     * Similarly, if no URI is provided, but a type hint for a valid plugin is
     * present, that plugin will be instantiated
     *
     * Otherwise a request to that URI is made and the type sent by the server
     * is used to find a suitable handler, EXCEPT when:
     *  - The type hint refers to a *supported* plugin, in which case that
     *    plugin will be instantiated regardless of the server provided type
     *  - The server returns a binary-stream type, and our type hint refers to
     *    a valid non-document type, we will use the type hint
     *
     * @param aNotify    If we should send notifications. If false, content
     *                   loading may be deferred while appropriate frames are
     *                   created
     * @param aForceLoad If we should reload this content (and re-attempt the
     *                   channel open) even if our parameters did not change
     */
    nsresult LoadObject(bool aNotify,
                        bool aForceLoad = false);

    enum Capabilities {
      eSupportImages       = PR_BIT(0), // Images are supported (imgILoader)
      eSupportPlugins      = PR_BIT(1), // Plugins are supported (nsIPluginHost)
      eSupportDocuments    = PR_BIT(2), // Documents are supported
                                        // (nsIDocumentLoaderFactory)
                                        // This flag always includes SVG
      eSupportSVG          = PR_BIT(3), // SVG is supported (image/svg+xml)
      eSupportClassID      = PR_BIT(4), // The classid attribute is supported

      // Allows us to load a plugin if it matches a MIME type or file extension
      // registered to a plugin without opening its specified URI first. Can
      // result in launching plugins for URIs that return differing content
      // types. Plugins without URIs may instantiate regardless.
      // XXX(johns) this is our legacy behavior on <embed> tags, whereas object
      // will always open a channel and check its MIME if a URI is present.
      eAllowPluginSkipChannel  = PR_BIT(5)
    };

    /**
     * Returns the list of capabilities this content node supports. This is a
     * bitmask consisting of flags from the Capabilities enum.
     *
     * The default implementation supports all types but not
     * eSupportClassID or eAllowPluginSkipChannel
     */
    virtual PRUint32 GetCapabilities() const;

    /**
     * Destroys all loaded documents/plugins and releases references
     */
    void DestroyContent();

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
    // Object parameter changes returned by UpdateObjectParameters
    enum ParameterUpdateFlags {
      eParamNoChange           = 0,
      // Parameters that potentially affect the channel changed
      // - mOriginalURI, mOriginalContentType
      eParamChannelChanged     = PR_BIT(0),
      // Parameters that affect displayed content changed
      // - mURI, mContentType, mType, mBaseURI
      eParamStateChanged       = PR_BIT(1)
    };

    /**
     * Loads fallback content with the specified FallbackType
     *
     * @param aType   FallbackType value for type of fallback we're loading
     * @param aNotify Send notifications and events. If false, caller is
     *                responsible for doing so
     */
    void LoadFallback(FallbackType aType, bool aNotify);

    /**
     * Internal version of LoadObject that should only be used by this class
     * aLoadingChannel is passed by the LoadObject call from OnStartRequest,
     * primarily for sanity-preservation
     */
    nsresult LoadObject(bool aNotify,
                        bool aForceLoad,
                        nsIRequest *aLoadingChannel);

    /**
     * Introspects the object and sets the following member variables:
     * - mOriginalContentType : This is the type attribute on the element
     * - mOriginalURI         : The src or data attribute on the element
     * - mURI                 : The final URI, considering mChannel if
     *                          mChannelLoaded is set
     * - mContentType         : The final content type, considering mChannel if
     *                          mChannelLoaded is set
     * - mBaseURI             : The object's base URI, which may be set by the
     *                          object (codebase attribute)
     * - mType                : The type the object is determined to be based
     *                          on the above
     * 
     * NOTE The class assumes that mType is the currently loaded type at various
     *      points, so the caller of this function must take the appropriate
     *      actions to ensure this
     * 
     * NOTE This function does not perform security checks, only determining the
     *      requested type and parameters of the object.
     * 
     * @return Returns a bitmask of ParameterUpdateFlags values
     */
    ParameterUpdateFlags UpdateObjectParameters();

    void NotifyContentObjectWrapper();

    /**
     * Opens the channel pointed to by mURI into mChannel.
     *
     * @param aPolicyType The value to be passed to channelPolicy->SetLoadType
     */
    nsresult OpenChannel(PRInt32 aPolicyType);

    /**
     * Closes and releases references to mChannel and, if opened, mFinalListener
     */
    nsresult CloseChannel();

    /**
     * If this object is allowed to play plugin content, or if it would display
     * click-to-play instead.
     * NOTE that this does not actually check if the object is a loadable plugin
     */
    bool ShouldPlay(FallbackType &aReason);

    /**
     * Checks if a URI passes security checks and content policy, relative to
     * the current document's principal
     *
     * @param aURI                The URI to consider
     * @param aContentPolicy      [in/out] A pointer to the initial content
     *                            policy, that will be updated to contain the
     *                            final determined policy
     * @param aContentPolicyType  The 'contentType' parameter passed to
     *                            NS_CheckContentLoadPolicy
     *
     * @return true if this URI is acceptable for loading
     */
    bool CheckURILoad(nsIURI *aURI,
                      PRInt16 *aContentPolicy,
                      PRInt32 aContentPolicyType);

    /**
     * Checks if the current mURI and mBaseURI pass content policy and security
     * checks for loading
     *
     * @param aContentPolicy     [in/out] A pointer to the initial content
     *                           policy, that will be updated to contain the
     *                           final determined policy if a URL is rejected
     * @param aContentPolicyType The 'contentType' parameter passed to
     *                           NS_CheckContentLoadPolicy
     *
     * @return true if the URIs are acceptable for loading
     */
    bool CheckObjectURIs(PRInt16 *aContentPolicy, PRInt32 aContentPolicyType);

    /**
     * Checks whether the given type is a supported document type
     *
     * NOTE Does not take content policy or capabilities into account
     */
    bool IsSupportedDocument(const nsCString& aType);

    /**
     * Unloads all content and resets the object to a completely unloaded state
     *
     * NOTE Calls StopPluginInstance() and may spin the event loop
     *
     * @param aResetState Reset the object type to 'loading' and destroy channel
     *                    as well
     */
    void UnloadObject(bool aResetState = true);

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
     * Fires the nsPluginErrorEvent. This function doesn't do any checks
     * whether it should be fired, or whether the given state translates to a
     * meaningful event
     */
    void FirePluginError(FallbackType aFallbackType);

    /**
     * Returns a ObjectType value corresponding to the type of content we would
     * support the given MIME type as, taking capabilities and plugin state
     * into account
     * 
     * NOTE this does not consider whether the content would be suppressed by
     *      click-to-play or other content policy checks
     */
    ObjectType GetTypeOfContent(const nsCString& aMIMEType);

    /**
     * For a classid, returns the MIME type that can be used to instantiate
     * a plugin for this ID.
     *
     * @param aClassID The class ID in question
     * @param aType    [out] The corresponding type, if the call is successful
     * @return NS_ERROR_NOT_AVAILABLE Unsupported class ID.
     */
    nsresult TypeForClassID(const nsAString& aClassID, nsACString& aType);

    /**
     * Gets the frame that's associated with this content node.
     * Does not flush.
     */
    nsObjectFrame* GetExistingFrame();

    // The final listener for mChannel (uriloader, pluginstreamlistener, etc.)
    nsCOMPtr<nsIStreamListener> mFinalListener;

    // Frame loader, for content documents we load.
    nsRefPtr<nsFrameLoader>     mFrameLoader;

    // A pending nsAsyncInstantiateEvent (may be null).  This is a weak ref.
    nsIRunnable                *mPendingInstantiateEvent;

    // The content type of our current load target, updated by
    // UpdateObjectParameters(). Takes the channel's type into account once
    // opened.
    //
    // May change if a channel is opened, does not imply a loaded state
    nsCString                   mContentType;

    // The content type 'hint' provided by the element's type attribute. May
    // or may not be used as a final type
    nsCString                   mOriginalContentType;

    // The channel that's currently being loaded. If set, but mChannelLoaded is
    // false, has not yet reached OnStartRequest
    nsCOMPtr<nsIChannel>        mChannel;

    // The URI of the current content.
    // May change as we open channels and encounter redirects - does not imply
    // a loaded type
    nsCOMPtr<nsIURI>            mURI;

    // The original URI obtained from inspecting the element (codebase, and
    // src/data). May differ from mURI due to redirects
    nsCOMPtr<nsIURI>            mOriginalURI;

    // The baseURI used for constructing mURI, and used by some plugins (java)
    // as a root for other resource requests.
    nsCOMPtr<nsIURI>            mBaseURI;



    // Type of the currently-loaded content.
    ObjectType                  mType           : 8;
    // The type of fallback content we're showing (see ObjectState())
    FallbackType                mFallbackType : 8;

    // If true, the current load has finished opening a channel. Does not imply
    // mChannel -- mChannelLoaded && !mChannel may occur for a load that failed
    bool                        mChannelLoaded    : 1;

    // Whether we are about to call instantiate on our frame. If we aren't,
    // SetFrame needs to asynchronously call Instantiate.
    bool                        mInstantiating : 1;

    // True when the object is created for an element which the parser has
    // created using NS_FROM_PARSER_NETWORK flag. If the element is modified,
    // it may lose the flag.
    bool                        mNetworkCreated : 1;

    // Used to keep track of whether or not a plugin has been explicitly
    // activated by PlayPlugin(). (see ShouldPlay())
    bool                        mActivated : 1;

    // Protects DoStopPlugin from reentry (bug 724781).
    bool                        mIsStopping : 1;

    // Protects LoadObject from re-entry
    bool                        mIsLoading : 1;

    // Used to track when we might try to instantiate a plugin instance based on
    // a src data stream being delivered to this object. When this is true we
    // don't want plugin instance instantiation code to attempt to load src data
    // again or we'll deliver duplicate streams. Should be cleared when we are
    // not loading src data.
    bool mSrcStreamLoading;


    nsWeakFrame                 mPrintFrame;

    nsRefPtr<nsPluginInstanceOwner> mInstanceOwner;
};

#endif
