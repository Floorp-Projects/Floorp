/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set et cin sw=2 sts=2:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla <object> loading code.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * A base class implementING nsIObjectLoadingContent for use by
 * various content nodes that want to provide plugin/document/image
 * loading functionality (eg <embed>, <object>, <applet>, etc).
 */

#ifndef NSOBJECTLOADINGCONTENT_H_
#define NSOBJECTLOADINGCONTENT_H_

#include "nsImageLoadingContent.h"
#include "nsIStreamListener.h"
#include "nsIFrameLoader.h"
#include "nsIInterfaceRequestor.h"
#include "nsIChannelEventSink.h"
#include "nsIObjectLoadingContent.h"
#include "nsIRunnable.h"

#include "nsWeakReference.h"

struct nsAsyncInstantiateEvent;
class  AutoNotifier;
class  AutoFallback;
class  AutoSetInstantiatingToFalse;

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
                             // Plugins code wants a weak reference to
                             // notification callbacks
                             , public nsSupportsWeakReference
{
  friend class AutoNotifier;
  friend class AutoFallback;
  friend class AutoSetInstantiatingToFalse;

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
    PRInt32 ObjectState() const;

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
                        PRBool aNotify,
                        const nsCString& aTypeHint = EmptyCString(),
                        PRBool aForceLoad = PR_FALSE);
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
                        PRBool aNotify,
                        const nsCString& aTypeHint = EmptyCString(),
                        PRBool aForceLoad = PR_FALSE);

    enum Capabilities {
      eSupportImages    = PR_BIT(0), // Images are supported (imgILoader)
      eSupportPlugins   = PR_BIT(1), // Plugins are supported (nsIPluginHost)
      eSupportDocuments = PR_BIT(2), // Documents are supported
                                     // (nsIDocumentLoaderFactory)
                                     // This flag always includes SVG
#ifdef MOZ_SVG
      eSupportSVG       = PR_BIT(3), // SVG is supported (image/svg+xml)
#endif
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
    void Fallback(PRBool aNotify);

    /**
     * Subclasses must call this function when they are removed from the
     * document.
     *
     * XXX This is a temporary workaround for docshell suckyness
     */
    void RemovedFromDocument();

    void Traverse(nsCycleCollectionTraversalCallback &cb);

  private:
    /**
     * Check whether the given request represents a successful load.
     */
    static PRBool IsSuccessfulRequest(nsIRequest* aRequest);

    /**
     * Check whether the URI can be handled internally.
     */
    static PRBool CanHandleURI(nsIURI* aURI);

    /**
     * Checks whether the given type is a supported document type.
     */
    PRBool IsSupportedDocument(const nsCString& aType);

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
     */
    void NotifyStateChanged(ObjectType aOldType, PRInt32 aOldState,
                            PRBool aSync);

    /**
     * Fires the "Plugin not found" event. This function doesn't do any checks
     * whether it should be fired, the caller should do that.
     */
    static void FirePluginNotFound(nsIContent* thisContent);

    ObjectType GetTypeOfContent(const nsCString& aMIMEType);

    /**
     * For a classid, returns the MIME type that can be used to instantiate
     * a plugin for this ID.
     *
     * @return NS_ERROR_NOT_AVAILABLE Unsupported class ID.
     */
    nsresult TypeForClassID(const nsAString& aClassID, nsACString& aType);

    /**
     * Gets the base URI to be used for this object. This differs from
     * nsIContent::GetBaseURI in that it takes codebase attributes into
     * account.
     */
    void GetObjectBaseURI(nsIContent* thisContent, nsIURI** aURI);

    /**
     * Gets the frame that's associated with this content node in
     * presentation 0.
     */
    nsIObjectFrame* GetFrame();

    /**
     * Instantiates the plugin. This differs from GetFrame()->Instantiate() in
     * that it ensures that the URI will be non-null, and that a MIME type
     * will be passed.
     */
    nsresult Instantiate(const nsACString& aMIMEType, nsIURI* aURI);

    /**
     * Whether to treat this content as a plugin, even though we can't handle
     * the type. This function impl should match the checks in the plugin host.
     * aContentType is the MIME type we ended up with.
     */
    static PRBool ShouldShowDefaultPlugin(nsIContent* aContent,
                                          const nsCString& aContentType);

    enum PluginSupportState {
      ePluginUnsupported,  // The plugin is not supported (not installed, say)
      ePluginDisabled,     // The plugin has been explicitly disabled by the
                           // user.
      ePluginOtherState    // Something else (e.g. not a plugin at all as far
                           // as we can tell).
    };

    /**
     * Get the plugin support state for the given content node and MIME type.
     * This is used for purposes of determining whether to fire PluginNotFound
     * events etc.  aContentType is the MIME type we ended up with.
     *
     * This should only be called if the type of this content is eType_Null.
     */
    static PluginSupportState
      GetPluginSupportState(nsIContent* aContent,
                            const nsCString& aContentType);

    /**
     * If the plugin for aContentType is disabled, return ePluginDisabled.
     * Otherwise (including if there is no plugin for aContentType at all),
     * return ePluginUnsupported.
     *
     * This should only be called if the type of this content is eType_Null.
     */
    static PluginSupportState
      GetPluginDisabledState(const nsCString& aContentType);

    /**
     * The final listener to ship the data to (imagelib, uriloader, etc)
     */
    nsCOMPtr<nsIStreamListener> mFinalListener;

    /**
     * Frame loader, for content documents we load.
     */
    nsCOMPtr<nsIFrameLoader>    mFrameLoader;

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
    PRBool                      mInstantiating : 1;
    // Blocking status from content policy
    PRBool                      mUserDisabled  : 1;
    PRBool                      mSuppressed    : 1;
    // Whether we fell back because of an unsupported type
    PRBool                      mTypeUnsupported:1;

    friend struct nsAsyncInstantiateEvent;
};


#endif
