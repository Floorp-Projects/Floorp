/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2007, Parakey Inc.
 * All rights reserved.
 *
 * Redistribution and use of this software in source and binary forms,
 * with or without modification, are permitted provided that the
 * following conditions are met:
 *
 * * Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the
 *   following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the
 *   following disclaimer in the documentation and/or other
 *   materials provided with the distribution.
 *
 * * Neither the name of Parakey Inc. nor the names of its
 *   contributors may be used to endorse or promote products
 *   derived from this software without specific prior
 *   written permission of Parakey Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Creator:
 *  Joe Hewitt
 * Contributors
 *  John J. Barton (IBM Almaden)
 *  Jan Odvarko (Mozilla Corp.)
 *  Max Stepanov (Aptana Inc.)
 *  Rob Campbell (Mozilla Corp.)
 *  Hans Hillen (Paciello Group, Mozilla)
 *  Curtis Bartley (Mozilla Corp.)
 *  Mike Collins (IBM Almaden)
 *  Kevin Decker
 *  Mike Ratcliffe (Comartis AG)
 *  Hernan Rodríguez Colmeiro
 *  Austin Andrews
 *  Christoph Dorn
 *  Steven Roussey (AppCenter Inc, Network54)
 *  Mihai Sucan (Mozilla Corp.)
 */

"use strict";

const { components, Cc, Ci, Cu } = require("chrome");
loader.lazyImporter(this, "NetUtil", "resource://gre/modules/NetUtil.jsm");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");
const Services = require("Services");

loader.lazyGetter(this, "certDecoder", () => {
  const { asn1js } = Cu.import(
    "chrome://global/content/certviewer/asn1js_bundle.js"
  );
  const { pkijs } = Cu.import(
    "chrome://global/content/certviewer/pkijs_bundle.js"
  );
  const { pvutils } = Cu.import(
    "chrome://global/content/certviewer/pvutils_bundle.js"
  );

  const { Integer, fromBER } = asn1js.asn1js;
  const { Certificate } = pkijs.pkijs;
  const { fromBase64, stringToArrayBuffer } = pvutils.pvutils;

  const { certDecoderInitializer } = Cu.import(
    "chrome://global/content/certviewer/certDecoder.js"
  );
  const { parse, pemToDER } = certDecoderInitializer(
    Integer,
    fromBER,
    Certificate,
    fromBase64,
    stringToArrayBuffer,
    crypto
  );
  return { parse, pemToDER };
});

// "Lax", "Strict" and "None" are special values of the SameSite cookie
// attribute that should not be translated.
const COOKIE_SAMESITE = {
  LAX: "Lax",
  STRICT: "Strict",
  NONE: "None",
};

/**
 * Helper object for networking stuff.
 *
 * Most of the following functions have been taken from the Firebug source. They
 * have been modified to match the Firefox coding rules.
 */
var NetworkHelper = {
  /**
   * Converts text with a given charset to unicode.
   *
   * @param string text
   *        Text to convert.
   * @param string charset
   *        Charset to convert the text to.
   * @returns string
   *          Converted text.
   */
  convertToUnicode: function(text, charset) {
    const conv = Cc[
      "@mozilla.org/intl/scriptableunicodeconverter"
    ].createInstance(Ci.nsIScriptableUnicodeConverter);
    try {
      conv.charset = charset || "UTF-8";
      return conv.ConvertToUnicode(text);
    } catch (ex) {
      return text;
    }
  },

  /**
   * Reads all available bytes from stream and converts them to charset.
   *
   * @param nsIInputStream stream
   * @param string charset
   * @returns string
   *          UTF-16 encoded string based on the content of stream and charset.
   */
  readAndConvertFromStream: function(stream, charset) {
    let text = null;
    try {
      text = NetUtil.readInputStreamToString(stream, stream.available());
      return this.convertToUnicode(text, charset);
    } catch (err) {
      return text;
    }
  },

  /**
   * Reads the posted text from request.
   *
   * @param nsIHttpChannel request
   * @param string charset
   *        The content document charset, used when reading the POSTed data.
   * @returns string or null
   *          Returns the posted string if it was possible to read from request
   *          otherwise null.
   */
  readPostTextFromRequest: function(request, charset) {
    if (request instanceof Ci.nsIUploadChannel) {
      const iStream = request.uploadStream;

      let isSeekableStream = false;
      if (iStream instanceof Ci.nsISeekableStream) {
        isSeekableStream = true;
      }

      let prevOffset;
      if (isSeekableStream) {
        prevOffset = iStream.tell();
        iStream.seek(Ci.nsISeekableStream.NS_SEEK_SET, 0);
      }

      // Read data from the stream.
      const text = this.readAndConvertFromStream(iStream, charset);

      // Seek locks the file, so seek to the beginning only if necko hasn't
      // read it yet, since necko doesn't seek to 0 before reading (at lest
      // not till 459384 is fixed).
      if (isSeekableStream && prevOffset == 0) {
        iStream.seek(Ci.nsISeekableStream.NS_SEEK_SET, 0);
      }
      return text;
    }
    return null;
  },

  /**
   * Reads the posted text from the page's cache.
   *
   * @param nsIDocShell docShell
   * @param string charset
   * @returns string or null
   *          Returns the posted string if it was possible to read from
   *          docShell otherwise null.
   */
  readPostTextFromPage: function(docShell, charset) {
    const webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
    return this.readPostTextFromPageViaWebNav(webNav, charset);
  },

  /**
   * Reads the posted text from the page's cache, given an nsIWebNavigation
   * object.
   *
   * @param nsIWebNavigation webNav
   * @param string charset
   * @returns string or null
   *          Returns the posted string if it was possible to read from
   *          webNav, otherwise null.
   */
  readPostTextFromPageViaWebNav: function(webNav, charset) {
    if (webNav instanceof Ci.nsIWebPageDescriptor) {
      const descriptor = webNav.currentDescriptor;

      if (
        descriptor instanceof Ci.nsISHEntry &&
        descriptor.postData &&
        descriptor instanceof Ci.nsISeekableStream
      ) {
        descriptor.seek(Ci.nsISeekableStream.NS_SEEK_SET, 0);

        return this.readAndConvertFromStream(descriptor, charset);
      }
    }
    return null;
  },

  /**
   * Gets the topFrameElement that is associated with request. This
   * works in single-process and multiprocess contexts. It may cross
   * the content/chrome boundary.
   *
   * @param nsIHttpChannel request
   * @returns Element|null
   *          The top frame element for the given request.
   */
  getTopFrameForRequest: function(request) {
    try {
      return this.getRequestLoadContext(request).topFrameElement;
    } catch (ex) {
      // request loadContent is not always available.
    }
    return null;
  },

  /**
   * Gets the nsIDOMWindow that is associated with request.
   *
   * @param nsIHttpChannel request
   * @returns nsIDOMWindow or null
   */
  getWindowForRequest: function(request) {
    try {
      return this.getRequestLoadContext(request).associatedWindow;
    } catch (ex) {
      // TODO: bug 802246 - getWindowForRequest() throws on b2g: there is no
      // associatedWindow property.
    }
    return null;
  },

  /**
   * Gets the nsILoadContext that is associated with request.
   *
   * @param nsIHttpChannel request
   * @returns nsILoadContext or null
   */
  getRequestLoadContext: function(request) {
    try {
      return request.notificationCallbacks.getInterface(Ci.nsILoadContext);
    } catch (ex) {
      // Ignore.
    }

    try {
      return request.loadGroup.notificationCallbacks.getInterface(
        Ci.nsILoadContext
      );
    } catch (ex) {
      // Ignore.
    }

    return null;
  },

  /**
   * Determines whether the request has been made for the top level document.
   *
   * @param nsIHttpChannel request
   * @returns Boolean True if the request represents the top level document.
   */
  isTopLevelLoad: function(request) {
    if (request instanceof Ci.nsIChannel) {
      const loadInfo = request.loadInfo;
      if (loadInfo?.isTopLevelLoad) {
        return request.loadFlags & Ci.nsIChannel.LOAD_DOCUMENT_URI;
      }
    }

    return false;
  },

  /**
   * Loads the content of url from the cache.
   *
   * @param string url
   *        URL to load the cached content for.
   * @param string charset
   *        Assumed charset of the cached content. Used if there is no charset
   *        on the channel directly.
   * @param function callback
   *        Callback that is called with the loaded cached content if available
   *        or null if something failed while getting the cached content.
   */
  loadFromCache: function(url, charset, callback) {
    const channel = NetUtil.newChannel({
      uri: url,
      loadUsingSystemPrincipal: true,
    });

    // Ensure that we only read from the cache and not the server.
    channel.loadFlags =
      Ci.nsIRequest.LOAD_FROM_CACHE |
      Ci.nsICachingChannel.LOAD_ONLY_FROM_CACHE |
      Ci.nsICachingChannel.LOAD_BYPASS_LOCAL_CACHE_IF_BUSY;

    NetUtil.asyncFetch(channel, (inputStream, statusCode, request) => {
      if (!components.isSuccessCode(statusCode)) {
        callback(null);
        return;
      }

      // Try to get the encoding from the channel. If there is none, then use
      // the passed assumed charset.
      const requestChannel = request.QueryInterface(Ci.nsIChannel);
      const contentCharset = requestChannel.contentCharset || charset;

      // Read the content of the stream using contentCharset as encoding.
      callback(this.readAndConvertFromStream(inputStream, contentCharset));
    });
  },

  /**
   * Parse a raw Cookie header value.
   *
   * @param string header
   *        The raw Cookie header value.
   * @return array
   *         Array holding an object for each cookie. Each object holds the
   *         following properties: name and value.
   */
  parseCookieHeader: function(header) {
    const cookies = header.split(";");
    const result = [];

    cookies.forEach(function(cookie) {
      const equal = cookie.indexOf("=");
      const name = cookie.substr(0, equal);
      const value = cookie.substr(equal + 1);
      result.push({
        name: unescape(name.trim()),
        value: unescape(value.trim()),
      });
    });

    return result;
  },

  /**
   * Parse a raw Set-Cookie header value.
   *
   * @param string header
   *        The raw Set-Cookie header value.
   * @return array
   *         Array holding an object for each cookie. Each object holds the
   *         following properties: name, value, secure (boolean), httpOnly
   *         (boolean), path, domain, samesite and expires (ISO date string).
   */
  parseSetCookieHeader: function(header) {
    function parseSameSiteAttribute(attribute) {
      attribute = attribute.toLowerCase();
      switch (attribute) {
        case COOKIE_SAMESITE.LAX.toLowerCase():
          return COOKIE_SAMESITE.LAX;
        case COOKIE_SAMESITE.STRICT.toLowerCase():
          return COOKIE_SAMESITE.STRICT;
        default:
          return COOKIE_SAMESITE.NONE;
      }
    }

    const rawCookies = header.split(/\r\n|\n|\r/);
    const cookies = [];

    rawCookies.forEach(function(cookie) {
      const equal = cookie.indexOf("=");
      const name = unescape(cookie.substr(0, equal).trim());
      const parts = cookie.substr(equal + 1).split(";");
      const value = unescape(parts.shift().trim());

      cookie = { name: name, value: value };

      parts.forEach(function(part) {
        part = part.trim();
        if (part.toLowerCase() == "secure") {
          cookie.secure = true;
        } else if (part.toLowerCase() == "httponly") {
          cookie.httpOnly = true;
        } else if (part.indexOf("=") > -1) {
          const pair = part.split("=");
          pair[0] = pair[0].toLowerCase();
          if (pair[0] == "path" || pair[0] == "domain") {
            cookie[pair[0]] = pair[1];
          } else if (pair[0] == "samesite") {
            cookie[pair[0]] = parseSameSiteAttribute(pair[1]);
          } else if (pair[0] == "expires") {
            try {
              pair[1] = pair[1].replace(/-/g, " ");
              cookie.expires = new Date(pair[1]).toISOString();
            } catch (ex) {
              // Ignore.
            }
          }
        }
      });

      cookies.push(cookie);
    });

    return cookies;
  },

  // This is a list of all the mime category maps jviereck could find in the
  // firebug code base.
  mimeCategoryMap: {
    "text/plain": "txt",
    "text/html": "html",
    "text/xml": "xml",
    "text/xsl": "txt",
    "text/xul": "txt",
    "text/css": "css",
    "text/sgml": "txt",
    "text/rtf": "txt",
    "text/x-setext": "txt",
    "text/richtext": "txt",
    "text/javascript": "js",
    "text/jscript": "txt",
    "text/tab-separated-values": "txt",
    "text/rdf": "txt",
    "text/xif": "txt",
    "text/ecmascript": "js",
    "text/vnd.curl": "txt",
    "text/x-json": "json",
    "text/x-js": "txt",
    "text/js": "txt",
    "text/vbscript": "txt",
    "view-source": "txt",
    "view-fragment": "txt",
    "application/xml": "xml",
    "application/xhtml+xml": "xml",
    "application/atom+xml": "xml",
    "application/rss+xml": "xml",
    "application/vnd.mozilla.maybe.feed": "xml",
    "application/javascript": "js",
    "application/x-javascript": "js",
    "application/x-httpd-php": "txt",
    "application/rdf+xml": "xml",
    "application/ecmascript": "js",
    "application/http-index-format": "txt",
    "application/json": "json",
    "application/x-js": "txt",
    "application/x-mpegurl": "txt",
    "application/vnd.apple.mpegurl": "txt",
    "multipart/mixed": "txt",
    "multipart/x-mixed-replace": "txt",
    "image/svg+xml": "svg",
    "application/octet-stream": "bin",
    "image/jpeg": "image",
    "image/jpg": "image",
    "image/gif": "image",
    "image/png": "image",
    "image/bmp": "image",
    "application/x-shockwave-flash": "flash",
    "video/x-flv": "flash",
    "audio/mpeg3": "media",
    "audio/x-mpeg-3": "media",
    "video/mpeg": "media",
    "video/x-mpeg": "media",
    "video/vnd.mpeg.dash.mpd": "xml",
    "audio/ogg": "media",
    "application/ogg": "media",
    "application/x-ogg": "media",
    "application/x-midi": "media",
    "audio/midi": "media",
    "audio/x-mid": "media",
    "audio/x-midi": "media",
    "music/crescendo": "media",
    "audio/wav": "media",
    "audio/x-wav": "media",
    "text/json": "json",
    "application/x-json": "json",
    "application/json-rpc": "json",
    "application/x-web-app-manifest+json": "json",
    "application/manifest+json": "json",
  },

  /**
   * Check if the given MIME type is a text-only MIME type.
   *
   * @param string mimeType
   * @return boolean
   */
  isTextMimeType: function(mimeType) {
    if (mimeType.indexOf("text/") == 0) {
      return true;
    }

    // XML and JSON often come with custom MIME types, so in addition to the
    // standard "application/xml" and "application/json", we also look for
    // variants like "application/x-bigcorp+xml". For JSON we allow "+json" and
    // "-json" as suffixes.
    if (/^application\/\w+(?:[\.-]\w+)*(?:\+xml|[-+]json)$/.test(mimeType)) {
      return true;
    }

    const category = this.mimeCategoryMap[mimeType] || null;
    switch (category) {
      case "txt":
      case "js":
      case "json":
      case "css":
      case "html":
      case "svg":
      case "xml":
        return true;

      default:
        return false;
    }
  },

  /**
   * Takes a securityInfo object of nsIRequest, the nsIRequest itself and
   * extracts security information from them.
   *
   * @param object securityInfo
   *        The securityInfo object of a request. If null channel is assumed
   *        to be insecure.
   * @param object httpActivity
   *        The httpActivity object for the request with at least members
   *        { private, hostname }.
   * @param Map decodedCertificateCache
   *        A Map of certificate fingerprints to decoded certificates, to avoid
   *        repeatedly decoding previously-seen certificates.
   *
   * @return object
   *         Returns an object containing following members:
   *          - state: The security of the connection used to fetch this
   *                   request. Has one of following string values:
   *                    * "insecure": the connection was not secure (only http)
   *                    * "weak": the connection has minor security issues
   *                    * "broken": secure connection failed (e.g. expired cert)
   *                    * "secure": the connection was properly secured.
   *          If state == broken:
   *            - errorMessage: error code string.
   *          If state == secure:
   *            - protocolVersion: one of TLSv1, TLSv1.1, TLSv1.2, TLSv1.3.
   *            - cipherSuite: the cipher suite used in this connection.
   *            - cert: information about certificate used in this connection.
   *                    See parseCertificateInfo for the contents.
   *            - hsts: true if host uses Strict Transport Security,
   *                    false otherwise
   *            - hpkp: true if host uses Public Key Pinning, false otherwise
   *          If state == weak: Same as state == secure and
   *            - weaknessReasons: list of reasons that cause the request to be
   *                               considered weak. See getReasonsForWeakness.
   */
  parseSecurityInfo: async function(
    securityInfo,
    httpActivity,
    decodedCertificateCache
  ) {
    const info = {
      state: "insecure",
    };

    // The request did not contain any security info.
    if (!securityInfo) {
      return info;
    }

    /**
     * Different scenarios to consider here and how they are handled:
     * - request is HTTP, the connection is not secure
     *   => securityInfo is null
     *      => state === "insecure"
     *
     * - request is HTTPS, the connection is secure
     *   => .securityState has STATE_IS_SECURE flag
     *      => state === "secure"
     *
     * - request is HTTPS, the connection has security issues
     *   => .securityState has STATE_IS_INSECURE flag
     *   => .errorCode is an NSS error code.
     *      => state === "broken"
     *
     * - request is HTTPS, the connection was terminated before the security
     *   could be validated
     *   => .securityState has STATE_IS_INSECURE flag
     *   => .errorCode is NOT an NSS error code.
     *   => .errorMessage is not available.
     *      => state === "insecure"
     *
     * - request is HTTPS but it uses a weak cipher or old protocol, see
     *   https://hg.mozilla.org/mozilla-central/annotate/def6ed9d1c1a/
     *   security/manager/ssl/nsNSSCallbacks.cpp#l1233
     * - request is mixed content (which makes no sense whatsoever)
     *   => .securityState has STATE_IS_BROKEN flag
     *   => .errorCode is NOT an NSS error code
     *   => .errorMessage is not available
     *      => state === "weak"
     */

    const wpl = Ci.nsIWebProgressListener;
    const NSSErrorsService = Cc["@mozilla.org/nss_errors_service;1"].getService(
      Ci.nsINSSErrorsService
    );
    if (!NSSErrorsService.isNSSErrorCode(securityInfo.errorCode)) {
      const state = securityInfo.securityState;

      let uri = null;
      if (httpActivity.channel?.URI) {
        uri = httpActivity.channel.URI;
      }
      if (uri && !uri.schemeIs("https") && !uri.schemeIs("wss")) {
        // it is not enough to look at the transport security info -
        // schemes other than https and wss are subject to
        // downgrade/etc at the scheme level and should always be
        // considered insecure
        info.state = "insecure";
      } else if (state & wpl.STATE_IS_SECURE) {
        // The connection is secure if the scheme is sufficient
        info.state = "secure";
      } else if (state & wpl.STATE_IS_BROKEN) {
        // The connection is not secure, there was no error but there's some
        // minor security issues.
        info.state = "weak";
        info.weaknessReasons = this.getReasonsForWeakness(state);
      } else if (state & wpl.STATE_IS_INSECURE) {
        // This was most likely an https request that was aborted before
        // validation. Return info as info.state = insecure.
        return info;
      } else {
        DevToolsUtils.reportException(
          "NetworkHelper.parseSecurityInfo",
          "Security state " + state + " has no known STATE_IS_* flags."
        );
        return info;
      }

      // Cipher suite.
      info.cipherSuite = securityInfo.cipherName;

      // Key exchange group name.
      info.keaGroupName = securityInfo.keaGroupName;

      // Certificate signature scheme.
      info.signatureSchemeName = securityInfo.signatureSchemeName;

      // Protocol version.
      info.protocolVersion = this.formatSecurityProtocol(
        securityInfo.protocolVersion
      );

      // Certificate.
      info.cert = await this.parseCertificateInfo(
        securityInfo.serverCert,
        decodedCertificateCache
      );

      // Certificate transparency status.
      info.certificateTransparency = securityInfo.certificateTransparencyStatus;

      // HSTS and HPKP if available.
      if (httpActivity.hostname) {
        const sss = Cc["@mozilla.org/ssservice;1"].getService(
          Ci.nsISiteSecurityService
        );
        const pkps = Cc[
          "@mozilla.org/security/publickeypinningservice;1"
        ].getService(Ci.nsIPublicKeyPinningService);

        // SiteSecurityService uses different storage if the channel is
        // private. Thus we must give isSecureURI correct flags or we
        // might get incorrect results.
        const flags = httpActivity.private
          ? Ci.nsISocketProvider.NO_PERMANENT_STORAGE
          : 0;

        if (!uri) {
          // isSecureURI only cares about the host, not the scheme.
          const host = httpActivity.hostname;
          uri = Services.io.newURI("https://" + host);
        }

        info.hsts = sss.isSecureURI(uri, flags);
        info.hpkp = pkps.hostHasPins(uri);
      } else {
        DevToolsUtils.reportException(
          "NetworkHelper.parseSecurityInfo",
          "Could not get HSTS/HPKP status as hostname is not available."
        );
        info.hsts = false;
        info.hpkp = false;
      }
    } else {
      // The connection failed.
      info.state = "broken";
      info.errorMessage = securityInfo.errorCodeString;
    }

    return info;
  },

  /**
   * Takes an nsIX509Cert and returns an object with certificate information.
   *
   * @param nsIX509Cert cert
   *        The certificate to extract the information from.
   * @param Map decodedCertificateCache
   *        A Map of certificate fingerprints to decoded certificates, to avoid
   *        repeatedly decoding previously-seen certificates.
   * @return object
   *         An object with following format:
   *           {
   *             subject: { commonName, organization, organizationalUnit },
   *             issuer: { commonName, organization, organizationUnit },
   *             validity: { start, end },
   *             fingerprint: { sha1, sha256 }
   *           }
   */
  parseCertificateInfo: async function(cert, decodedCertificateCache) {
    function getDNComponent(dn, componentType) {
      for (const [type, value] of dn.entries) {
        if (type == componentType) {
          return value;
        }
      }
      return undefined;
    }

    const info = {};
    if (cert) {
      const certHash = cert.sha256Fingerprint;
      let parsedCert = decodedCertificateCache.get(certHash);
      if (!parsedCert) {
        parsedCert = await certDecoder.parse(
          certDecoder.pemToDER(cert.getBase64DERString())
        );
        decodedCertificateCache.set(certHash, parsedCert);
      }
      info.subject = {
        commonName: getDNComponent(parsedCert.subject, "Common Name"),
        organization: getDNComponent(parsedCert.subject, "Organization"),
        organizationalUnit: getDNComponent(
          parsedCert.subject,
          "Organizational Unit"
        ),
      };

      info.issuer = {
        commonName: getDNComponent(parsedCert.issuer, "Common Name"),
        organization: getDNComponent(parsedCert.issuer, "Organization"),
        organizationUnit: getDNComponent(
          parsedCert.issuer,
          "Organizational Unit"
        ),
      };

      info.validity = {
        start: parsedCert.notBeforeUTC,
        end: parsedCert.notAfterUTC,
      };

      info.fingerprint = {
        sha1: parsedCert.fingerprint.sha1,
        sha256: parsedCert.fingerprint.sha256,
      };
    } else {
      DevToolsUtils.reportException(
        "NetworkHelper.parseCertificateInfo",
        "Secure connection established without certificate."
      );
    }

    return info;
  },

  /**
   * Takes protocolVersion of TransportSecurityInfo object and returns
   * human readable description.
   *
   * @param Number version
   *        One of nsITransportSecurityInfo version constants.
   * @return string
   *         One of TLSv1, TLSv1.1, TLSv1.2, TLSv1.3 if @param version
   *         is valid, Unknown otherwise.
   */
  formatSecurityProtocol: function(version) {
    switch (version) {
      case Ci.nsITransportSecurityInfo.TLS_VERSION_1:
        return "TLSv1";
      case Ci.nsITransportSecurityInfo.TLS_VERSION_1_1:
        return "TLSv1.1";
      case Ci.nsITransportSecurityInfo.TLS_VERSION_1_2:
        return "TLSv1.2";
      case Ci.nsITransportSecurityInfo.TLS_VERSION_1_3:
        return "TLSv1.3";
      default:
        DevToolsUtils.reportException(
          "NetworkHelper.formatSecurityProtocol",
          "protocolVersion " + version + " is unknown."
        );
        return "Unknown";
    }
  },

  /**
   * Takes the securityState bitfield and returns reasons for weak connection
   * as an array of strings.
   *
   * @param Number state
   *        nsITransportSecurityInfo.securityState.
   *
   * @return Array[String]
   *         List of weakness reasons. A subset of { cipher } where
   *         * cipher: The cipher suite is consireded to be weak (RC4).
   */
  getReasonsForWeakness: function(state) {
    const wpl = Ci.nsIWebProgressListener;

    // If there's non-fatal security issues the request has STATE_IS_BROKEN
    // flag set. See https://hg.mozilla.org/mozilla-central/file/44344099d119
    // /security/manager/ssl/nsNSSCallbacks.cpp#l1233
    const reasons = [];

    if (state & wpl.STATE_IS_BROKEN) {
      const isCipher = state & wpl.STATE_USES_WEAK_CRYPTO;

      if (isCipher) {
        reasons.push("cipher");
      }

      if (!isCipher) {
        DevToolsUtils.reportException(
          "NetworkHelper.getReasonsForWeakness",
          "STATE_IS_BROKEN without a known reason. Full state was: " + state
        );
      }
    }

    return reasons;
  },

  /**
   * Parse a url's query string into its components
   *
   * @param string queryString
   *        The query part of a url
   * @return array
   *         Array of query params {name, value}
   */
  parseQueryString: function(queryString) {
    // Make sure there's at least one param available.
    // Be careful here, params don't necessarily need to have values, so
    // no need to verify the existence of a "=".
    if (!queryString) {
      return null;
    }

    // Turn the params string into an array containing { name: value } tuples.
    const paramsArray = queryString
      .replace(/^[?&]/, "")
      .split("&")
      .map(e => {
        const param = e.split("=");
        return {
          name: param[0]
            ? NetworkHelper.convertToUnicode(unescape(param[0]))
            : "",
          value: param[1]
            ? NetworkHelper.convertToUnicode(unescape(param[1]))
            : "",
        };
      });

    return paramsArray;
  },
};

for (const prop of Object.getOwnPropertyNames(NetworkHelper)) {
  exports[prop] = NetworkHelper[prop];
}
