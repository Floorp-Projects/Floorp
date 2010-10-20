/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et :
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
 * The Original Code is mozilla.org code.
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

/**
 * This is the base class for all link classes.
 */

#ifndef mozilla_dom_Link_h__
#define mozilla_dom_Link_h__

#include "nsIContent.h"

namespace mozilla {
namespace dom {

#define MOZILLA_DOM_LINK_IMPLEMENTATION_IID \
  { 0xa687a99c, 0x3893, 0x45c0, \
    {0x8e, 0xab, 0xb8, 0xf7, 0xd7, 0x9e, 0x9e, 0x7b } }

class Link : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(MOZILLA_DOM_LINK_IMPLEMENTATION_IID)

  static const nsLinkState defaultState = eLinkState_Unknown;
  Link();
  nsLinkState GetLinkState() const;
  virtual void SetLinkState(nsLinkState aState);

  /**
   * @return NS_EVENT_STATE_VISITED if this link is visited,
   *         NS_EVENT_STATE_UNVISTED if this link is not visited, or 0 if this
   *         link is not actually a link.
   */
  nsEventStates LinkState() const;

  /**
   * @return the URI this link is for, if available.
   */
  already_AddRefed<nsIURI> GetURI() const;
  virtual already_AddRefed<nsIURI> GetURIExternal() const {
    return GetURI();
  }

  /**
   * Helper methods for modifying and obtaining parts of the URI of the Link.
   */
  nsresult SetProtocol(const nsAString &aProtocol);
  nsresult SetHost(const nsAString &aHost);
  nsresult SetHostname(const nsAString &aHostname);
  nsresult SetPathname(const nsAString &aPathname);
  nsresult SetSearch(const nsAString &aSearch);
  nsresult SetPort(const nsAString &aPort);
  nsresult SetHash(const nsAString &aHash);
  nsresult GetProtocol(nsAString &_protocol);
  nsresult GetHost(nsAString &_host);
  nsresult GetHostname(nsAString &_hostname);
  nsresult GetPathname(nsAString &_pathname);
  nsresult GetSearch(nsAString &_search);
  nsresult GetPort(nsAString &_port);
  nsresult GetHash(nsAString &_hash);

  /**
   * Invalidates any link caching, and resets the state to the default.
   *
   * @param aNotify
   *        true if ResetLinkState should notify the owning document about style
   *        changes or false if it should not.
   */
  void ResetLinkState(bool aNotify);

protected:
  virtual ~Link();

  bool HasCachedURI() const { return !!mCachedURI; }

private:
  /**
   * Unregisters from History so this node no longer gets notifications about
   * changes to visitedness.
   */
  void UnregisterFromHistory();

  already_AddRefed<nsIURI> GetURIToMutate();
  void SetHrefAttribute(nsIURI *aURI);

  nsLinkState mLinkState;

  mutable nsCOMPtr<nsIURI> mCachedURI;

  bool mRegistered;

  /**
   * Obtains a pointer to the nsIContent interface that classes inheriting from
   * this should also inherit from.
   */
  nsIContent *Content();
  nsIContent *mContent;
};

NS_DEFINE_STATIC_IID_ACCESSOR(Link, MOZILLA_DOM_LINK_IMPLEMENTATION_IID)

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Link_h__
