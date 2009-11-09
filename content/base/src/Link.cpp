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

#include "Link.h"

#include "nsIEventStateManager.h"
#include "nsIURL.h"

#include "nsEscape.h"
#include "nsGkAtoms.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

Link::Link()
  : mLinkState(defaultState)
{
}

nsLinkState
Link::GetLinkState() const
{
  return mLinkState;
}

void
Link::SetLinkState(nsLinkState aState)
{
  mLinkState = aState;
}

PRInt32
Link::LinkState() const
{
  if (mLinkState == eLinkState_Visited) {
    return NS_EVENT_STATE_VISITED;
  }

  if (mLinkState == eLinkState_Unvisited) {
    return NS_EVENT_STATE_UNVISITED;
  }

  return 0;
}

already_AddRefed<nsIURI>
Link::GetURI() const
{
  nsCOMPtr<nsIURI> uri(mCachedURI);

  // If we have this URI cached, use it.
  if (uri) {
    return uri.forget();
  }

  // Otherwise obtain it.
  Link *self = const_cast<Link *>(this);
  nsCOMPtr<nsIContent> content(do_QueryInterface(self));
  NS_ASSERTION(content, "Why isn't this an nsIContent node?!");
  uri = content->GetHrefURI();

  // We want to cache the URI if the node is in the document.
  if (uri && content->IsInDoc()) {
    mCachedURI = uri;
  }

  return uri.forget();
}

nsresult
Link::SetProtocol(const nsAString &aProtocol)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return NS_OK;
  }

  nsAString::const_iterator start, end;
  aProtocol.BeginReading(start);
  aProtocol.EndReading(end);
  nsAString::const_iterator iter(start);
  (void)FindCharInReadable(':', iter, end);
  (void)uri->SetScheme(NS_ConvertUTF16toUTF8(Substring(start, iter)));

  SetHrefAttribute(uri);
  return NS_OK;
}

nsresult
Link::SetHost(const nsAString &aHost)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return NS_OK;
  }

  // We cannot simply call nsIURI::SetHost because that would treat the name as
  // an IPv6 address (like http:://[server:443]/).  We also cannot call
  // nsIURI::SetHostPort because that isn't implemented.  Sadfaces.

  // First set the hostname.
  nsAString::const_iterator start, end;
  aHost.BeginReading(start);
  aHost.EndReading(end);
  nsAString::const_iterator iter(start);
  (void)FindCharInReadable(':', iter, end);
  NS_ConvertUTF16toUTF8 host(Substring(start, iter));
  (void)uri->SetHost(host);

  // Also set the port if needed.
  if (iter != end) {
    iter++;
    if (iter != end) {
      nsAutoString portStr(Substring(iter, end));
      nsresult rv;
      PRInt32 port = portStr.ToInteger((PRInt32 *)&rv);
      if (NS_SUCCEEDED(rv)) {
        (void)uri->SetPort(port);
      }
    }
  };

  SetHrefAttribute(uri);
  return NS_OK;
}

nsresult
Link::SetHostname(const nsAString &aHostname)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return NS_OK;
  }

  (void)uri->SetHost(NS_ConvertUTF16toUTF8(aHostname));
  SetHrefAttribute(uri);
  return NS_OK;
}

nsresult
Link::SetPathname(const nsAString &aPathname)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (!url) {
    // Ignore failures to be compatible with NS4.
    return NS_OK;
  }

  (void)url->SetFilePath(NS_ConvertUTF16toUTF8(aPathname));
  SetHrefAttribute(uri);
  return NS_OK;
}

nsresult
Link::SetSearch(const nsAString &aSearch)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (!url) {
    // Ignore failures to be compatible with NS4.
    return NS_OK;
  }

  (void)url->SetQuery(NS_ConvertUTF16toUTF8(aSearch));
  SetHrefAttribute(uri);
  return NS_OK;
}

nsresult
Link::SetPort(const nsAString &aPort)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  if (!uri) {
    // Ignore failures to be compatible with NS4.
    return NS_OK;
  }

  nsresult rv;
  nsAutoString portStr(aPort);
  PRInt32 port = portStr.ToInteger((PRInt32 *)&rv);
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  (void)uri->SetPort(port);
  SetHrefAttribute(uri);
  return NS_OK;
}

nsresult
Link::SetHash(const nsAString &aHash)
{
  nsCOMPtr<nsIURI> uri(GetURIToMutate());
  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (!url) {
    // Ignore failures to be compatible with NS4.
    return NS_OK;
  }

  (void)url->SetRef(NS_ConvertUTF16toUTF8(aHash));
  SetHrefAttribute(uri);
  return NS_OK;
}

nsresult
Link::GetProtocol(nsAString &_protocol)
{
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    _protocol.AssignLiteral("http");
  }
  else {
    nsCAutoString scheme;
    (void)uri->GetScheme(scheme);
    CopyASCIItoUTF16(scheme, _protocol);
  }
  _protocol.Append(PRUnichar(':'));
  return NS_OK;
}

nsresult
Link::GetHost(nsAString &_host)
{
  _host.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Do not throw!  Not having a valid URI should result in an empty string.
    return NS_OK;
  }

  nsCAutoString hostport;
  nsresult rv = uri->GetHostPort(hostport);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(hostport, _host);
  }
  return NS_OK;
}

nsresult
Link::GetHostname(nsAString &_hostname)
{
  _hostname.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Do not throw!  Not having a valid URI should result in an empty string.
    return NS_OK;
  }

  nsCAutoString host;
  nsresult rv = uri->GetHost(host);
  // Note that failure to get the host from the URI is not necessarily a bad
  // thing.  Some URIs do not have a host.
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(host, _hostname);
  }
  return NS_OK;
}

nsresult
Link::GetPathname(nsAString &_pathname)
{
  _pathname.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (!url) {
    // Do not throw!  Not having a valid URI or URL should result in an empty
    // string.
    return NS_OK;
  }

  nsCAutoString file;
  nsresult rv = url->GetFilePath(file);
  NS_ENSURE_SUCCESS(rv, rv);
  CopyUTF8toUTF16(file, _pathname);
  return NS_OK;
}

nsresult
Link::GetSearch(nsAString &_search)
{
  _search.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (!url) {
    // Do not throw!  Not having a valid URI or URL should result in an empty
    // string.
    return NS_OK;
  }

  nsCAutoString search;
  nsresult rv = url->GetQuery(search);
  if (NS_SUCCEEDED(rv) && !search.IsEmpty()) {
    CopyUTF8toUTF16(NS_LITERAL_CSTRING("?") + search, _search);
  }
  return NS_OK;
}

nsresult
Link::GetPort(nsAString &_port)
{
  _port.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    // Do not throw!  Not having a valid URI should result in an empty string.
    return NS_OK;
  }

  PRInt32 port;
  nsresult rv = uri->GetPort(&port);
  // Note that failure to get the port from the URI is not necessarily a bad
  // thing.  Some URIs do not have a port.
  if (NS_SUCCEEDED(rv) && port != -1) {
    nsAutoString portStr;
    portStr.AppendInt(port, 10);
    _port.Assign(portStr);
  }
  return NS_OK;
}

nsresult
Link::GetHash(nsAString &_hash)
{
  _hash.Truncate();

  nsCOMPtr<nsIURI> uri(GetURI());
  nsCOMPtr<nsIURL> url(do_QueryInterface(uri));
  if (!url) {
    // Do not throw!  Not having a valid URI or URL should result in an empty
    // string.
    return NS_OK;
  }

  nsCAutoString ref;
  nsresult rv = url->GetRef(ref);
  if (NS_SUCCEEDED(rv) && !ref.IsEmpty()) {
    NS_UnescapeURL(ref); // XXX may result in random non-ASCII bytes!
    _hash.Assign(PRUnichar('#'));
    AppendUTF8toUTF16(ref, _hash);
  }
  return NS_OK;
}

void
Link::ResetLinkState()
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(this));
  NS_ASSERTION(content, "Why isn't this an nsIContent node?!");

  // Tell the document to forget about this link.
  nsIDocument *doc = content->GetCurrentDoc();
  if (doc) {
    doc->ForgetLink(content);
  }

  // Update our state back to the default.
  mLinkState = defaultState;

  // Get rid of our cached URI.
  mCachedURI = nsnull;
}

already_AddRefed<nsIURI>
Link::GetURIToMutate()
{
  nsCOMPtr<nsIURI> uri(GetURI());
  if (!uri) {
    return nsnull;
  }
  nsCOMPtr<nsIURI> clone;
  (void)uri->Clone(getter_AddRefs(clone));
  return clone.forget();
}

void
Link::SetHrefAttribute(nsIURI *aURI)
{
  NS_ASSERTION(aURI, "Null URI is illegal!");
  nsCOMPtr<nsIContent> content(do_QueryInterface(this));
  NS_ASSERTION(content, "Why isn't this an nsIContent node?!");

  nsCAutoString href;
  (void)aURI->GetSpec(href);
  (void)content->SetAttr(kNameSpaceID_None, nsGkAtoms::href,
                         NS_ConvertUTF8toUTF16(href), PR_TRUE);
}

} // namespace dom
} // namespace mozilla
