/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A base class implementing nsIObjectLoadingContent for use by
 * various content nodes that want to provide plugin/document/image
 * loading functionality (eg <embed>, <object>, etc).
 */

#ifndef NSOBJECTLOADINGCONTENT_H_
#define NSOBJECTLOADINGCONTENT_H_

#include "mozilla/Maybe.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsIFrame.h"  // for WeakFrame only
#include "nsIStreamListener.h"
#include "nsIChannelEventSink.h"
#include "nsIObjectLoadingContent.h"
#include "nsFrameLoaderOwner.h"

class nsStopPluginRunnable;
class nsIPrincipal;
class nsFrameLoader;

namespace mozilla::dom {
struct BindContext;
class FeaturePolicy;
template <typename T>
class Sequence;
class HTMLIFrameElement;
template <typename T>
struct Nullable;
class WindowProxyHolder;
class XULFrameElement;
}  // namespace mozilla::dom

class nsObjectLoadingContent : public nsIStreamListener,
                               public nsFrameLoaderOwner,
                               public nsIObjectLoadingContent,
                               public nsIChannelEventSink {
  friend class AutoSetLoadingToFalse;

 public:
  // This enum's values must be the same as the constants on
  // nsIObjectLoadingContent
  enum class ObjectType : uint8_t {
    // Loading, type not yet known. We may be waiting for a channel to open.
    Loading = TYPE_LOADING,
    // Content is a subdocument, possibly SVG
    Document = TYPE_DOCUMENT,
    // Content is unknown and should be represented by an empty element.
    Fallback = TYPE_FALLBACK
  };

  nsObjectLoadingContent();
  virtual ~nsObjectLoadingContent();

  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIOBJECTLOADINGCONTENT
  NS_DECL_NSICHANNELEVENTSINK

  ObjectType Type() const { return mType; }

  void SetIsNetworkCreated(bool aNetworkCreated) {
    mNetworkCreated = aNetworkCreated;
  }

  static bool IsSuccessfulRequest(nsIRequest*, nsresult* aStatus);

  // WebIDL API
  mozilla::dom::Document* GetContentDocument(nsIPrincipal& aSubjectPrincipal);
  void GetActualType(nsAString& aType) const {
    CopyUTF8toUTF16(mContentType, aType);
  }
  uint32_t DisplayedType() const { return uint32_t(mType); }
  nsIURI* GetSrcURI() const { return mURI; }

  void SwapFrameLoaders(mozilla::dom::HTMLIFrameElement& aOtherLoaderOwner,
                        mozilla::ErrorResult& aRv) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  }
  void SwapFrameLoaders(mozilla::dom::XULFrameElement& aOtherLoaderOwner,
                        mozilla::ErrorResult& aRv) {
    aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  }

  bool IsRewrittenYoutubeEmbed() const { return mRewrittenYoutubeEmbed; }

  const mozilla::Maybe<mozilla::IntrinsicSize>& GetSubdocumentIntrinsicSize()
      const {
    return mSubdocumentIntrinsicSize;
  }

  const mozilla::Maybe<mozilla::AspectRatio>& GetSubdocumentIntrinsicRatio()
      const {
    return mSubdocumentIntrinsicRatio;
  }

  void SubdocumentIntrinsicSizeOrRatioChanged(
      const mozilla::Maybe<mozilla::IntrinsicSize>& aIntrinsicSize,
      const mozilla::Maybe<mozilla::AspectRatio>& aIntrinsicRatio);

  void SubdocumentImageLoadComplete(nsresult aResult);

 protected:
  /**
   * Begins loading the object when called
   *
   * Attributes of |this| QI'd to nsIContent will be inspected, depending on
   * the node type. This function currently assumes it is a <object> or
   * <embed> tag.
   *
   * The instantiated plugin depends on:
   * - The URI (<embed src>, <object data>)
   * - The type 'hint' (type attribute)
   * - The mime type returned by opening the URI
   * - Enabled plugins claiming the ultimate mime type
   * - The capabilities returned by GetCapabilities
   * - The classid attribute, if eFallbackIfClassIDPresent is among the
   * capabilities
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
  nsresult LoadObject(bool aNotify, bool aForceLoad = false);

  enum Capabilities {
    eSupportImages = 1u << 0,     // Images are supported (imgILoader)
    eSupportDocuments = 1u << 1,  // Documents are supported
                                  // (DocumentLoaderFactory)
                                  // This flag always includes SVG

    // Node supports class ID as an attribute, and should fallback if it is
    // present, as class IDs are not supported.
    eFallbackIfClassIDPresent = 1u << 2,

    // If possible to get a *plugin* type from the type attribute *or* file
    // extension, we can use that type and begin loading the plugin before
    // opening a channel.
    // A side effect of this is if the channel fails, the plugin is still
    // running.
    eAllowPluginSkipChannel = 1u << 3
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
  void Destroy();

  // Subclasses should call cycle collection methods from the respective
  // traverse / unlink.
  static void Traverse(nsObjectLoadingContent* tmp,
                       nsCycleCollectionTraversalCallback& cb);
  static void Unlink(nsObjectLoadingContent* tmp);

  void CreateStaticClone(nsObjectLoadingContent* aDest) const;

  void UnbindFromTree();

  /**
   * Return the content policy type used for loading the element.
   */
  virtual nsContentPolicyType GetContentPolicyType() const = 0;

  virtual const mozilla::dom::Element* AsElement() const = 0;
  mozilla::dom::Element* AsElement() {
    return const_cast<mozilla::dom::Element*>(
        const_cast<const nsObjectLoadingContent*>(this)->AsElement());
  }

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

  /**
   * Updates and stores the container's feature policy in its canonical browsing
   * context. This gets called whenever the feature policy has changed, which
   * can happen when this element is upgraded to a container or when the URI of
   * the element has changed.
   */
  void RefreshFeaturePolicy();

 private:
  // Object parameter changes returned by UpdateObjectParameters
  enum ParameterUpdateFlags {
    eParamNoChange = 0,
    // Parameters that potentially affect the channel changed
    // - mOriginalURI, mOriginalContentType
    eParamChannelChanged = 1u << 0,
    // Parameters that affect displayed content changed
    // - mURI, mContentType, mType, mBaseURI
    eParamStateChanged = 1u << 1,
    // The effective content type changed, independant of object type. This
    // can happen when changing from Loading -> Final type, but doesn't
    // necessarily happen when changing between object types. E.g., if a PDF
    // handler was installed between the last load of this object and now, we
    // might change from Document -> Plugin without changing
    // ContentType
    eParamContentTypeChanged = 1u << 2
  };

  /**
   * If we're an <object>, and show fallback, we might need to start nested
   * <embed> or <object> loads that would otherwise be blocked by
   * BlockEmbedOrObjectContentLoading().
   */
  void TriggerInnerFallbackLoads();

  /**
   * Internal version of LoadObject that should only be used by this class
   * aLoadingChannel is passed by the LoadObject call from OnStartRequest,
   * primarily for sanity-preservation
   */
  nsresult LoadObject(bool aNotify, bool aForceLoad,
                      nsIRequest* aLoadingChannel);

  /**
   * Inspects the object and sets the following member variables:
   * - mOriginalContentType : This is the type attribute on the element
   * - mOriginalURI         : The src or data attribute on the element
   * - mURI                 : The final URI, considering mChannel if
   *                          mChannelLoaded is set
   * - mContentType         : The final content type, considering mChannel if
   *                          mChannelLoaded is set
   * - mBaseURI             : The object's base URI, which may be set by the
   *                          object
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

 public:
  bool IsAboutBlankLoadOntoInitialAboutBlank(nsIURI* aURI,
                                             bool aInheritPrincipal,
                                             nsIPrincipal* aPrincipalToInherit);

 private:
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
   * This method tells the final answer on whether this object's fallback
   * content should be used instead of the original plugin content.
   *
   * @param aIsPluginClickToPlay Whether this object instance is CTP.
   */
  bool PreferFallback(bool aIsPluginClickToPlay);

  /**
   * Helper to check if our current URI passes policy
   *
   * @param aContentPolicy [out] The result of the content policy decision
   *
   * @return true if call succeeded and NS_CP_ACCEPTED(*aContentPolicy)
   */
  bool CheckLoadPolicy(int16_t* aContentPolicy);

  /**
   * Helper to check if the object passes process policy. Assumes we have a
   * final determined type.
   *
   * @param aContentPolicy [out] The result of the content policy decision
   *
   * @return true if call succeeded and NS_CP_ACCEPTED(*aContentPolicy)
   */
  bool CheckProcessPolicy(int16_t* aContentPolicy);

  void SetupFrameLoader();

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
   * @param aNotify if false, only need to update the state of our element.
   */
  void NotifyStateChanged(ObjectType aOldType, bool aNotify);

  /**
   * Returns a ObjectType value corresponding to the type of content we would
   * support the given MIME type as, taking capabilities and plugin state
   * into account
   *
   * @return The ObjectType enum value that we would attempt to load
   *
   * NOTE this does not consider whether the content would be suppressed by
   *      click-to-play or other content policy checks
   */
  ObjectType GetTypeOfContent(const nsCString& aMIMEType);

  /**
   * Used for identifying whether we can rewrite a youtube flash embed to
   * possibly use HTML5 instead.
   *
   * Returns true if plugin.rewrite_youtube_embeds pref is true and the
   * element this nsObjectLoadingContent instance represents:
   *
   * - is an embed or object node
   * - has a URL pointing at the youtube.com domain, using "/v/" style video
   *   path reference.
   *
   * Having the enablejsapi flag means the document that contains the element
   * could possibly be manipulating the youtube video elsewhere on the page
   * via javascript. In the context of embed elements, this usage has been
   * deprecated by youtube, so we can just rewrite as normal.
   *
   * If we can rewrite the URL, we change the "/v/" to "/embed/", and change
   * our type to Document so that we render similarly to an iframe
   * embed.
   */
  void MaybeRewriteYoutubeEmbed(nsIURI* aURI, nsIURI* aBaseURI,
                                nsIURI** aRewrittenURI);

  // Utility for firing an error event, if we're an <object>.
  void MaybeFireErrorEvent();

  /**
   * Store feature policy in container browsing context so that it can be
   * accessed cross process.
   */
  void MaybeStoreCrossOriginFeaturePolicy();

  /**
   * Return the value of either `data` or `src`, depending on element type,
   * parsed as a URL. If URL is invalid or the attribute is missing this returns
   * the document's origin.
   */
  static already_AddRefed<nsIPrincipal> GetFeaturePolicyDefaultOrigin(
      nsINode* aNode);

  // The final listener for mChannel (uriloader, pluginstreamlistener, etc.)
  nsCOMPtr<nsIStreamListener> mFinalListener;

  // The content type of our current load target, updated by
  // UpdateObjectParameters(). Takes the channel's type into account once
  // opened.
  //
  // May change if a channel is opened, does not imply a loaded state
  nsCString mContentType;

  // The content type 'hint' provided by the element's type attribute. May
  // or may not be used as a final type
  nsCString mOriginalContentType;

  // The channel that's currently being loaded. If set, but mChannelLoaded is
  // false, has not yet reached OnStartRequest
  nsCOMPtr<nsIChannel> mChannel;

  // The URI of the current content.
  // May change as we open channels and encounter redirects - does not imply
  // a loaded type
  nsCOMPtr<nsIURI> mURI;

  // The original URI obtained from inspecting the element. May differ from
  // mURI due to redirects
  nsCOMPtr<nsIURI> mOriginalURI;

  // The baseURI used for constructing mURI.
  nsCOMPtr<nsIURI> mBaseURI;

  // Type of the currently-loaded content.
  ObjectType mType;

  // If true, we have opened a channel as the listener and it has reached
  // OnStartRequest. Does not get set for channels that are passed directly to
  // the plugin listener.
  bool mChannelLoaded : 1;

  // True when the object is created for an element which the parser has
  // created using NS_FROM_PARSER_NETWORK flag. If the element is modified,
  // it may lose the flag.
  bool mNetworkCreated : 1;

  // Whether content blocking is enabled or not for this object.
  bool mContentBlockingEnabled : 1;

  // Protects DoStopPlugin from reentry (bug 724781).
  bool mIsStopping : 1;

  // Protects LoadObject from re-entry
  bool mIsLoading : 1;

  // For plugin stand-in types (click-to-play) tracks whether content js has
  // tried to access the plugin script object.
  bool mScriptRequested : 1;

  // True if object represents an object/embed tag pointing to a flash embed
  // for a youtube video. When possible (see IsRewritableYoutubeEmbed function
  // comments for details), we change these to try to load HTML5 versions of
  // videos.
  bool mRewrittenYoutubeEmbed : 1;

  bool mLoadingSyntheticDocument : 1;

  // The intrinsic size and aspect ratio from a child SVG document that
  // we should use.  These are only set when we are an <object> or <embed>
  // and the inner document is SVG.
  //
  // We store these here rather than on nsSubDocumentFrame since we are
  // sometimes notified of our child's intrinsics before we've constructed
  // our own frame.
  mozilla::Maybe<mozilla::IntrinsicSize> mSubdocumentIntrinsicSize;
  mozilla::Maybe<mozilla::AspectRatio> mSubdocumentIntrinsicRatio;

  // This gets created on the first call of `RefreshFeaturePolicy`, and will be
  // kept after that. Navigations of this element will use this if they're
  // targetting documents, which is how iframe element works. If it's a
  // non-document the feature policy isn't used, but it doesn't hurt to keep it
  // around, and a subsequent document load will continue using it after
  // refreshing it.
  RefPtr<mozilla::dom::FeaturePolicy> mFeaturePolicy;
};

#endif
