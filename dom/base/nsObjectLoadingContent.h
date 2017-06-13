/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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

#include "mozilla/Attributes.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsImageLoadingContent.h"
#include "nsIStreamListener.h"
#include "nsIChannelEventSink.h"
#include "nsIContentPolicy.h"
#include "nsIObjectLoadingContent.h"
#include "nsIRunnable.h"
#include "nsIThreadInternal.h"
#include "nsIFrame.h"
#include "nsIFrameLoader.h"

class nsAsyncInstantiateEvent;
class nsStopPluginRunnable;
class AutoSetInstantiatingToFalse;
class nsIPrincipal;
class nsFrameLoader;
class nsPluginFrame;
class nsXULElement;
class nsPluginInstanceOwner;

namespace mozilla {
namespace dom {
template<typename T> class Sequence;
struct MozPluginParameter;
class HTMLIFrameElement;
} // namespace dom
} // namespace mozilla

class nsObjectLoadingContent : public nsImageLoadingContent
                             , public nsIStreamListener
                             , public nsIFrameLoaderOwner
                             , public nsIObjectLoadingContent
                             , public nsIChannelEventSink
{
  friend class AutoSetInstantiatingToFalse;
  friend class AutoSetLoadingToFalse;
  friend class CheckPluginStopEvent;
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
      // Content is a fake plugin, which loads as a document but behaves as a
      // plugin (see nsPluginHost::CreateFakePlugin)
      eType_FakePlugin     = TYPE_FAKE_PLUGIN,
      // Content is a subdocument, possibly SVG
      eType_Document       = TYPE_DOCUMENT,
      // No content loaded (fallback). May be showing alternate content or
      // a custom error handler - *including* click-to-play dialogs
      eType_Null           = TYPE_NULL
    };

    enum FallbackType {
      // The content type is not supported (e.g. plugin not installed)
      eFallbackUnsupported = nsIObjectLoadingContent::PLUGIN_UNSUPPORTED,
      // Showing alternate content
      eFallbackAlternate = nsIObjectLoadingContent::PLUGIN_ALTERNATE,
      // The plugin exists, but is disabled
      eFallbackDisabled = nsIObjectLoadingContent::PLUGIN_DISABLED,
      // The plugin is blocklisted and disabled
      eFallbackBlocklisted = nsIObjectLoadingContent::PLUGIN_BLOCKLISTED,
      // The plugin is considered outdated, but not disabled
      eFallbackOutdated = nsIObjectLoadingContent::PLUGIN_OUTDATED,
      // The plugin has crashed
      eFallbackCrashed = nsIObjectLoadingContent::PLUGIN_CRASHED,
      // Suppressed by security policy
      eFallbackSuppressed = nsIObjectLoadingContent::PLUGIN_SUPPRESSED,
      // Blocked by content policy
      eFallbackUserDisabled = nsIObjectLoadingContent::PLUGIN_USER_DISABLED,
      /// ** All values >= eFallbackClickToPlay are plugin placeholder types
      ///    that would be replaced by a real plugin if activated (PlayPlugin())
      /// ** Furthermore, values >= eFallbackClickToPlay and
      ///    <= eFallbackVulnerableNoUpdate are click-to-play types.
      // The plugin is disabled until the user clicks on it
      eFallbackClickToPlay = nsIObjectLoadingContent::PLUGIN_CLICK_TO_PLAY,
      // The plugin is vulnerable (update available)
      eFallbackVulnerableUpdatable = nsIObjectLoadingContent::PLUGIN_VULNERABLE_UPDATABLE,
      // The plugin is vulnerable (no update available)
      eFallbackVulnerableNoUpdate = nsIObjectLoadingContent::PLUGIN_VULNERABLE_NO_UPDATE,
    };

    nsObjectLoadingContent();
    virtual ~nsObjectLoadingContent();

    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSIFRAMELOADEROWNER
    NS_DECL_NSIOBJECTLOADINGCONTENT
    NS_DECL_NSICHANNELEVENTSINK

    /**
     * Object state. This is a bitmask of NS_EVENT_STATEs epresenting the
     * current state of the object.
     */
    mozilla::EventStates ObjectState() const;

    ObjectType Type() const { return mType; }

    void SetIsNetworkCreated(bool aNetworkCreated)
    {
      mNetworkCreated = aNetworkCreated;
    }

    /**
     * When the object is loaded, the attributes and all nested <param>
     * elements are cached as name:value string pairs to be passed as
     * parameters when instantiating the plugin.
     *
     * Note: these cached values can be overriden for different quirk cases.
     */
    // Returns the cached attributes array.
    void GetPluginAttributes(nsTArray<mozilla::dom::MozPluginParameter>& aAttributes);

    // Returns the cached <param> array.
    void GetPluginParameters(nsTArray<mozilla::dom::MozPluginParameter>& aParameters);

    /**
     * Immediately instantiate a plugin instance. This is a no-op if mType !=
     * eType_Plugin or a plugin is already running.
     *
     * aIsLoading indicates that we are in the loading code, and we can bypass
     * the mIsLoading check.
     */
    nsresult InstantiatePluginInstance(bool aIsLoading = false);

    /**
     * Notify this class the document state has changed
     * Called by nsDocument so we may suspend plugins in inactive documents)
     */
    void NotifyOwnerDocumentActivityChanged();

    /**
     * When a plug-in is instantiated, it can create a scriptable
     * object that the page wants to interact with.  We expose this
     * object by placing it on the prototype chain of our element,
     * between the element itself and its most-derived DOM prototype.
     *
     * SetupProtoChain handles actually inserting the plug-in
     * scriptable object into the proto chain if needed.
     *
     * DoResolve is a hook that allows us to find out when the web
     * page is looking up a property name on our object and make sure
     * that our plug-in, if any, is instantiated.
     */
    // Helper for WebIDL node wrapping
    void SetupProtoChain(JSContext* aCx, JS::Handle<JSObject*> aObject);

    // Remove plugin from protochain
    void TeardownProtoChain();

    // Helper for WebIDL NeedResolve
    bool DoResolve(JSContext* aCx, JS::Handle<JSObject*> aObject,
                   JS::Handle<jsid> aId,
                   JS::MutableHandle<JS::PropertyDescriptor> aDesc);
    // The return value is whether DoResolve might end up resolving the given
    // id.  If in doubt, return true.
    static bool MayResolve(jsid aId);

    // Helper for WebIDL enumeration
    void GetOwnPropertyNames(JSContext* aCx, JS::AutoIdVector& /* unused */,
                             bool /* unused */, mozilla::ErrorResult& aRv);

    // WebIDL API
    nsIDocument* GetContentDocument(nsIPrincipal& aSubjectPrincipal);
    void GetActualType(nsAString& aType) const
    {
      CopyUTF8toUTF16(mContentType, aType);
    }
    uint32_t DisplayedType() const
    {
      return mType;
    }
    uint32_t GetContentTypeForMIMEType(const nsAString& aMIMEType)
    {
      return GetTypeOfContent(NS_ConvertUTF16toUTF8(aMIMEType), false);
    }
    void PlayPlugin(mozilla::dom::SystemCallerGuarantee,
                    mozilla::ErrorResult& aRv);
    void Reload(bool aClearActivation, mozilla::ErrorResult& aRv)
    {
      aRv = Reload(aClearActivation);
    }
    bool Activated() const
    {
      return mActivated;
    }
    nsIURI* GetSrcURI() const
    {
      return mURI;
    }

    /**
     * The default state that this plugin would be without manual activation.
     * @returns PLUGIN_ACTIVE if the default state would be active.
     */
    uint32_t DefaultFallbackType();

    uint32_t PluginFallbackType() const
    {
      return mFallbackType;
    }
    bool HasRunningPlugin() const
    {
      return !!mInstanceOwner;
    }
    // FIXME rename this
    void SkipFakePlugins(mozilla::ErrorResult& aRv)
    {
      aRv = SkipFakePlugins();
    }
    void SwapFrameLoaders(mozilla::dom::HTMLIFrameElement& aOtherLoaderOwner,
                          mozilla::ErrorResult& aRv)
    {
      aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    }
    void SwapFrameLoaders(nsXULElement& aOtherLoaderOwner,
                          mozilla::ErrorResult& aRv)
    {
      aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
    }
    void LegacyCall(JSContext* aCx, JS::Handle<JS::Value> aThisVal,
                    const mozilla::dom::Sequence<JS::Value>& aArguments,
                    JS::MutableHandle<JS::Value> aRetval,
                    mozilla::ErrorResult& aRv);

    uint32_t GetRunID(mozilla::dom::SystemCallerGuarantee,
                      mozilla::ErrorResult& aRv);

    bool IsRewrittenYoutubeEmbed() const
    {
      return mRewrittenYoutubeEmbed;
    }

    void PresetOpenerWindow(mozIDOMWindowProxy* aOpenerWindow, mozilla::ErrorResult& aRv);

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
      eSupportImages       = 1u << 0, // Images are supported (imgILoader)
      eSupportPlugins      = 1u << 1, // Plugins are supported (nsIPluginHost)
      eSupportDocuments    = 1u << 2, // Documents are supported
                                        // (nsIDocumentLoaderFactory)
                                        // This flag always includes SVG
      eSupportClassID      = 1u << 3, // The classid attribute is supported

      // If possible to get a *plugin* type from the type attribute *or* file
      // extension, we can use that type and begin loading the plugin before
      // opening a channel.
      // A side effect of this is if the channel fails, the plugin is still
      // running.
      eAllowPluginSkipChannel  = 1u << 4
    };

    /**
     * Returns the list of capabilities this content node supports. This is a
     * bitmask consisting of flags from the Capabilities enum.
     *
     * The default implementation supports all types but not
     * eSupportClassID or eAllowPluginSkipChannel
     */
    virtual uint32_t GetCapabilities() const;

    /**
     * Destroys all loaded documents/plugins and releases references
     */
    void DestroyContent();

    static void Traverse(nsObjectLoadingContent *tmp,
                         nsCycleCollectionTraversalCallback &cb);

    void CreateStaticClone(nsObjectLoadingContent* aDest) const;

    void DoStopPlugin(nsPluginInstanceOwner* aInstanceOwner);

    nsresult BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                        nsIContent* aBindingParent,
                        bool aCompileEventHandler);
    void UnbindFromTree(bool aDeep = true,
                        bool aNullParent = true);

    /**
     * Return the content policy type used for loading the element.
     */
    virtual nsContentPolicyType GetContentPolicyType() const = 0;

    /**
     * Decides whether we should load <embed>/<object> node content.
     *
     * If this is an <embed> or <object> node there are cases in which we should
     * not try to load the content:
     *
     * - If the node is the child of a media element
     * - If the node is the child of an <object> node that already has
     *   content being loaded.
     *
     * In these cases, this function will return false, which will cause
     * us to skip calling LoadObject.
     */
    bool BlockEmbedOrObjectContentLoading();

  private:

    // Object parameter changes returned by UpdateObjectParameters
    enum ParameterUpdateFlags {
      eParamNoChange           = 0,
      // Parameters that potentially affect the channel changed
      // - mOriginalURI, mOriginalContentType
      eParamChannelChanged     = 1u << 0,
      // Parameters that affect displayed content changed
      // - mURI, mContentType, mType, mBaseURI
      eParamStateChanged       = 1u << 1,
      // The effective content type changed, independant of object type. This
      // can happen when changing from Loading -> Final type, but doesn't
      // necessarily happen when changing between object types. E.g., if a PDF
      // handler was installed between the last load of this object and now, we
      // might change from eType_Document -> eType_Plugin without changing
      // ContentType
      eParamContentTypeChanged = 1u << 2
    };

    /**
     * Getter for child <param> elements that are not nested in another plugin
     * dom element.
     * This is an internal helper function and should not be used directly for
     * passing parameters to the plugin instance.
     *
     * See GetPluginParameters and GetPluginAttributes, which also handle
     * quirk-overrides.
     *
     * @param aParameters     The array containing pairs of name/value strings
     *                        from nested <param> objects.
     * @param aIgnoreCodebase Flag for ignoring the "codebase" param when
     *                        building the array. This is useful when loading
     *                        java.
     */
    void GetNestedParams(nsTArray<mozilla::dom::MozPluginParameter>& aParameters,
                         bool aIgnoreCodebase);

    MOZ_MUST_USE nsresult BuildParametersArray();

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
     * @param aJavaURI Specify that the URI will be consumed by java, which
     *                 changes codebase parsing and URI construction. Used
     *                 internally.
     *
     * @return Returns a bitmask of ParameterUpdateFlags values
     */
    ParameterUpdateFlags UpdateObjectParameters(bool aJavaURI = false);

    /**
     * Queue a CheckPluginStopEvent and track it in mPendingCheckPluginStopEvent
     */
    void QueueCheckPluginStopEvent();

    void NotifyContentObjectWrapper();

    /**
     * Opens the channel pointed to by mURI into mChannel.
     */
    nsresult OpenChannel();

    /**
     * Closes and releases references to mChannel and, if opened, mFinalListener
     */
    nsresult CloseChannel();

    /**
     * If this object should be tested against blocking list.
     */
    bool ShouldBlockContent();

    /**
     * If this object is allowed to play plugin content, or if it would display
     * click-to-play instead.
     * NOTE that this does not actually check if the object is a loadable plugin
     * NOTE This ignores the current activated state. The caller should check
     *      this if appropriate.
     */
    bool ShouldPlay(FallbackType &aReason);

    /**
     * This method tells if the fallback content should be attempted to be used
     * over the original object content.
     * It will look at prefs and this plugin's CTP state to make a decision.
     *
     * NOTE that this doesn't say whether the fallback _will_ be used, only whether
     * we should look into it to possibly use it. The final answer will be
     * given by the PreferFallback method.
     *
     * @param aIsPluginClickToPlay Whether this object instance is CTP.
     */
    bool FavorFallbackMode(bool aIsPluginClickToPlay);

    /**
     * Whether the page has provided good fallback content to this object.
     */
    bool HasGoodFallback();

    /**
     * This method tells the final answer on whether this object's fallback
     * content should be used instead of the original plugin content.
     *
     * @param aIsPluginClickToPlay Whether this object instance is CTP.
     */
    bool PreferFallback(bool aIsPluginClickToPlay);

    /*
     * Helper to check if mBaseURI can be used by java as a codebase
     */
    bool CheckJavaCodebase();

    /**
     * Helper to check if our current URI passes policy
     *
     * @param aContentPolicy [out] The result of the content policy decision
     *
     * @return true if call succeeded and NS_CP_ACCEPTED(*aContentPolicy)
     */
    bool CheckLoadPolicy(int16_t *aContentPolicy);

    /**
     * Helper to check if the object passes process policy. Assumes we have a
     * final determined type.
     *
     * @param aContentPolicy [out] The result of the content policy decision
     *
     * @return true if call succeeded and NS_CP_ACCEPTED(*aContentPolicy)
     */
    bool CheckProcessPolicy(int16_t *aContentPolicy);

    /**
     * Gets the plugin instance and creates a plugin stream listener, assigning
     * it to mFinalListener
     */
    bool MakePluginListener();

    void SetupFrameLoader(int32_t aJSPluginId);

    /**
     * Helper to spawn mFrameLoader and return a pointer to its docshell
     *
     * @param aURI URI we intend to load for the recursive load check (does not
     *             actually load anything)
     */
    already_AddRefed<nsIDocShell> SetupDocShell(nsIURI* aRecursionCheckURI);

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
    void NotifyStateChanged(ObjectType aOldType,
                            mozilla::EventStates aOldState,
                            bool aSync, bool aNotify);

    /**
     * Returns a ObjectType value corresponding to the type of content we would
     * support the given MIME type as, taking capabilities and plugin state
     * into account
     *
     * @param aNoFakePlugin Don't select a fake plugin handler as a valid type,
     *                      as when SkipFakePlugins() is called.
     * @return The ObjectType enum value that we would attempt to load
     *
     * NOTE this does not consider whether the content would be suppressed by
     *      click-to-play or other content policy checks
     */
    ObjectType GetTypeOfContent(const nsCString& aMIMEType, bool aNoFakePlugin);

    /**
     * Gets the frame that's associated with this content node.
     * Does not flush.
     */
    nsPluginFrame* GetExistingFrame();

    /**
     * Used for identifying whether we can rewrite a youtube flash embed to
     * possibly use HTML5 instead.
     *
     * Returns true if plugin.rewrite_youtube_embeds pref is true and the
     * element this nsObjectLoadingContent instance represents:
     *
     * - is an embed or object node
     * - has a URL pointing at the youtube.com domain, using "/v/" style video
     *   path reference, and without enablejsapi=1 in the path
     *
     * Having the enablejsapi flag means the document that contains the element
     * could possibly be manipulating the youtube video elsewhere on the page
     * via javascript. We can't rewrite these kinds of elements without possibly
     * breaking content, which we want to avoid.
     *
     * If we can rewrite the URL, we change the "/v/" to "/embed/", and change
     * our type to eType_Document so that we render similarly to an iframe
     * embed.
     */
    void MaybeRewriteYoutubeEmbed(nsIURI* aURI,
                                  nsIURI* aBaseURI,
                                  nsIURI** aRewrittenURI);

    // Helper class for SetupProtoChain
    class SetupProtoChainRunner final : public nsIRunnable
    {
      ~SetupProtoChainRunner() = default;
    public:
      NS_DECL_ISUPPORTS

      explicit SetupProtoChainRunner(nsObjectLoadingContent* aContent);

      NS_IMETHOD Run() override;

    private:
      // We store an nsIObjectLoadingContent because we can
      // unambiguously refcount that.
      RefPtr<nsIObjectLoadingContent> mContent;
    };

    // Utility getter for getting our nsNPAPIPluginInstance in a safe way.
    nsresult ScriptRequestPluginInstance(JSContext* aCx,
                                         nsNPAPIPluginInstance** aResult);

    // Utility method for getting our plugin JSObject
    static nsresult GetPluginJSObject(JSContext *cx,
                                      JS::Handle<JSObject*> obj,
                                      nsNPAPIPluginInstance *plugin_inst,
                                      JS::MutableHandle<JSObject*> plugin_obj,
                                      JS::MutableHandle<JSObject*> plugin_proto);

    // Utility for firing an error event, if we're an <object>.
    void MaybeFireErrorEvent();

    // The final listener for mChannel (uriloader, pluginstreamlistener, etc.)
    nsCOMPtr<nsIStreamListener> mFinalListener;

    // Frame loader, for content documents we load.
    RefPtr<nsFrameLoader>     mFrameLoader;

    // Track if we have a pending AsyncInstantiateEvent
    nsCOMPtr<nsIRunnable>       mPendingInstantiateEvent;

    // Tracks if we have a pending CheckPluginStopEvent
    nsCOMPtr<nsIRunnable>       mPendingCheckPluginStopEvent;

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

    uint32_t                    mRunID;
    bool                        mHasRunID : 1;

    // If true, we have opened a channel as the listener and it has reached
    // OnStartRequest. Does not get set for channels that are passed directly to
    // the plugin listener.
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

    // Whether content blocking is enabled or not for this object.
    bool                        mContentBlockingEnabled : 1;

    // If we should not use fake plugins until the next type change
    bool                        mSkipFakePlugins : 1;

    // Protects DoStopPlugin from reentry (bug 724781).
    bool                        mIsStopping : 1;

    // Protects LoadObject from re-entry
    bool                        mIsLoading : 1;

    // For plugin stand-in types (click-to-play) tracks whether content js has
    // tried to access the plugin script object.
    bool                        mScriptRequested : 1;

    // True if object represents an object/embed tag pointing to a flash embed
    // for a youtube video. When possible (see IsRewritableYoutubeEmbed function
    // comments for details), we change these to try to load HTML5 versions of
    // videos.
    bool                        mRewrittenYoutubeEmbed : 1;

    // Cache the answer of PreferFallback() because ShouldPlay is called several
    // times during the load process.
    bool                        mPreferFallback : 1;
    bool                        mPreferFallbackKnown : 1;

    WeakFrame                   mPrintFrame;

    RefPtr<nsPluginInstanceOwner> mInstanceOwner;
    nsTArray<mozilla::dom::MozPluginParameter> mCachedAttributes;
    nsTArray<mozilla::dom::MozPluginParameter> mCachedParameters;
};

#endif
