/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef mozilla_IHistory_h_
#define mozilla_IHistory_h_

#include "nsISupports.h"

class nsIURI;
class nsString;

namespace mozilla {

    namespace dom {
        class Link;
    }

#define IHISTORY_IID \
  {0x6f733924, 0x6321, 0x4384, {0x01, 0xee, 0x8e, 0x7d, 0xfb, 0xde, 0xe7, 0xa8}}

class IHistory : public nsISupports
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(IHISTORY_IID)

    /**
     * Registers the Link for notifications about the visited-ness of aURI.
     * Consumers should assume that the URI is unvisited after calling this, and
     * they will be notified if that state (unvisited) changes by having
     * SetLinkState called on themselves.  This function is guaranteed to run to
     * completion before aLink is notified.  After the node is notified, it will
     * be unregistered.
     *
     * @note SetLinkState must not call RegisterVisitedCallback or
     *       UnregisterVisitedCallback.
     *
     * @pre aURI must not be null.
     * @pre aLink may be null only in the parent (chrome) process.
     *
     * @param aURI
     *        The URI to check.
     * @param aLink
     *        The link to update whenever the history status changes.  The
     *        implementation will only hold onto a raw pointer, so if this
     *        object should be destroyed, be sure to call
     *        UnregisterVistedCallback first.
     */
    NS_IMETHOD RegisterVisitedCallback(nsIURI *aURI, dom::Link *aLink) = 0;

    /**
     * Unregisters a previously registered Link object.  This must be called
     * before destroying the registered object.
     *
     * @pre aURI must not be null.
     * @pre aLink must not be null.
     *
     * @param aURI
     *        The URI that aLink was registered for.
     * @param aLink
     *        The link object to unregister for aURI.
     */
    NS_IMETHOD UnregisterVisitedCallback(nsIURI *aURI, dom::Link *aLink) = 0;

    enum VisitFlags {
        /**
         * Indicates whether the URI was loaded in a top-level window.
         */
        TOP_LEVEL = 1 << 0,
        /**
         * Indicates whether the URI was loaded as part of a permanent redirect.
         */
        REDIRECT_PERMANENT = 1 << 1,
        /**
         * Indicates whether the URI was loaded as part of a temporary redirect.
         */
        REDIRECT_TEMPORARY = 1 << 2,
        /**
         * Indicates the URI is redirecting  (Response code 3xx).
         */
        REDIRECT_SOURCE = 1 << 3,
        /**
         * Indicates the URI caused an error that is unlikely fixable by a
         * retry, like a not found or unfetchable page.
         */
        UNRECOVERABLE_ERROR = 1 << 4
    };

    /**
     * Adds a history visit for the URI.
     *
     * @pre aURI must not be null.
     *
     * @param aURI
     *        The URI of the page being visited.
     * @param aLastVisitedURI
     *        The URI of the last visit in the chain.
     * @param aFlags
     *        The VisitFlags describing this visit.
     */
    NS_IMETHOD VisitURI(
        nsIURI *aURI,
        nsIURI *aLastVisitedURI,
        PRUint32 aFlags
    ) = 0;

    /**
     * Set the title of the URI.
     *
     * @pre aURI must not be null.
     *
     * @param aURI
     *        The URI to set the title for.
     * @param aTitle
     *        The title string.
     */
    NS_IMETHOD SetURITitle(nsIURI* aURI, const nsAString& aTitle) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(IHistory, IHISTORY_IID)

#define NS_DECL_IHISTORY \
    NS_IMETHOD RegisterVisitedCallback(nsIURI *aURI, \
                                       mozilla::dom::Link *aContent); \
    NS_IMETHOD UnregisterVisitedCallback(nsIURI *aURI, \
                                         mozilla::dom::Link *aContent); \
    NS_IMETHOD VisitURI(nsIURI *aURI, \
                        nsIURI *aLastVisitedURI, \
                        PRUint32 aFlags); \
    NS_IMETHOD SetURITitle(nsIURI* aURI, const nsAString& aTitle);

} // namespace mozilla

#endif // mozilla_IHistory_h_
