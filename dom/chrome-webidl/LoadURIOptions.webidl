/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

interface ContentSecurityPolicy;
interface Principal;
interface URI;
interface InputStream;
interface ReferrerInfo;

/**
 * This dictionary holds load arguments for docshell loads.
 */
[GenerateInit]
dictionary LoadURIOptions {
  /**
   * The principal that initiated the load.
   */
  Principal? triggeringPrincipal = null;

  /**
   * The CSP to be used for the load. That is *not* the CSP that will
   * be applied to subresource loads within that document but the CSP
   * for the document load itself. E.g. if that CSP includes
   * upgrade-insecure-requests, then the new top-level load will
   * be upgraded to HTTPS.
   */
  ContentSecurityPolicy? csp = null;

  /**
   * Flags modifying load behaviour.  This parameter is a bitwise
   * combination of the load flags defined in nsIWebNavigation.idl.
   */
   long loadFlags = 0;

  /**
   * The referring info of the load.  If this argument is null, then the
   * referrer URI and referrer policy will be inferred internally.
   */
   ReferrerInfo? referrerInfo = null;

  /**
   * If the URI to be loaded corresponds to a HTTP request, then this stream is
   * appended directly to the HTTP request headers.  It may be prefixed
   * with additional HTTP headers.  This stream must contain a "\r\n"
   * sequence separating any HTTP headers from the HTTP request body.
   */
  InputStream? postData = null;

  /**
   * If the URI corresponds to a HTTP request, then any HTTP headers
   * contained in this stream are set on the HTTP request.  The HTTP
   * header stream is formatted as:
   *     ( HEADER "\r\n" )*
   */
   InputStream? headers = null;

  /**
   * Set to indicate a base URI to be associated with the load. Note
   * that at present this argument is only used with view-source aURIs
   * and cannot be used to resolve aURI.
   */
  URI? baseURI = null;

  /**
   * Set to indicate that the URI to be loaded was triggered by a user
   * action. (Mostly used in the context of Sec-Fetch-User).
   */
  boolean hasValidUserGestureActivation = false;


  /**
  * The SandboxFlags of the entity thats
  * responsible for causing the load.
  */
  unsigned long triggeringSandboxFlags = 0;

  /**
  * The window id and storage access status of the window of the
  * context that triggered the load. This is used to allow self-initiated
  * same-origin navigations to propagate their "has storage access" bit
  * to the next Document.
  */
  unsigned long long triggeringWindowId = 0;
  boolean triggeringStorageAccess = false;

  /**
   * The RemoteType of the entity that's responsible for the load. Defaults to
   * the current process.
   *
   * When starting a load in a content process, `triggeringRemoteType` must be
   * either unset, or match the current remote type.
   */
  UTF8String? triggeringRemoteType;

  /**
   * If non-0, a value to pass to nsIDocShell::setCancelContentJSEpoch
   * when initiating the load.
   */
  long cancelContentJSEpoch = 0;

  /**
   * If this is passed, it will control which remote type is used to finish this
   * load. Ignored for non-`about:` loads.
   *
   * NOTE: This is _NOT_ defaulted to `null`, as `null` is the value for
   * `NOT_REMOTE_TYPE`, and we need to determine the difference between no
   * `remoteTypeOverride` and a `remoteTypeOverride` of `NOT_REMOTE_TYPE`.
   */
  UTF8String? remoteTypeOverride;

  /**
   * Whether the search/URL term was without an explicit scheme.
   */
  boolean wasSchemelessInput = false;
};
