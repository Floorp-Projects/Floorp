/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Anthony Ricaud <rik@webkit.org>
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2009 Mozilla Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

"use strict";

const Services = require("Services");

const Curl = {
  /**
   * Generates a cURL command string which can be used from the command line etc.
   *
   * @param object data
   *        Datasource to create the command from.
   *        The object must contain the following properties:
   *        - url:string, the URL of the request.
   *        - method:string, the request method upper cased. HEAD / GET / POST etc.
   *        - headers:array, an array of request headers {name:x, value:x} tuples.
   *        - httpVersion:string, http protocol version rfc2616 formatted. Eg. "HTTP/1.1"
   *        - postDataText:string, optional - the request payload.
   *
   * @param string platform
   *        Optional parameter to override platform,
   *        Fallbacks to current platform if not defined.
   *
   * @return string
   *         A cURL command.
   */
  generateCommand: function(data, platform) {
    const utils = CurlUtils;

    let command = ["curl"];

    // Make sure to use the following helpers to sanitize arguments before execution.
    const addParam = value => {
      const safe = /^[a-zA-Z-]+$/.test(value) ? value : escapeString(value);
      command.push(safe);
    };

    const addPostData = value => {
      const safe = /^[a-zA-Z-]+$/.test(value) ? value : escapeString(value);
      postData.push(safe);
    };

    const ignoredHeaders = new Set();
    const currentPlatform = platform || Services.appinfo.OS;

    // The cURL command is expected to run on the same platform that Firefox runs
    // (it may be different from the inspected page platform).
    const escapeString =
      currentPlatform == "WINNT"
        ? utils.escapeStringWin
        : utils.escapeStringPosix;

    // Add URL.
    addParam(data.url);

    // Disable globbing if the URL contains brackets.
    // cURL also globs braces but they are already percent-encoded.
    if (data.url.includes("[") || data.url.includes("]")) {
      addParam("--globoff");
    }

    let postDataText = null;
    const multipartRequest = utils.isMultipartRequest(data);

    // Create post data.
    const postData = [];
    if (multipartRequest) {
      // WINDOWS KNOWN LIMITATIONS: Due to the specificity of running curl on
      // cmd.exe even correctly escaped windows newline \r\n will be
      // treated by curl as plain local newline. It corresponds in unix
      // to single \n and that's what curl will send in payload.
      // It may be particularly hurtful for multipart/form-data payloads
      // which composed using \n only, not \r\n, may be not parsable for
      // peers which split parts of multipart payload using \r\n.
      postDataText = data.postDataText;
      addPostData("--data-binary");
      const boundary = utils.getMultipartBoundary(data);
      const text = utils.removeBinaryDataFromMultipartText(
        postDataText,
        boundary
      );
      addPostData(text);
      ignoredHeaders.add("content-length");
    } else if (
      data.postDataText &&
      (utils.isUrlEncodedRequest(data) ||
        ["PUT", "POST", "PATCH"].includes(data.method))
    ) {
      // When no postData exists, --data-raw should not be set
      postDataText = data.postDataText;
      addPostData("--data-raw");
      addPostData(utils.writePostDataTextParams(postDataText));
      ignoredHeaders.add("content-length");
    }
    // curl generates the host header itself based on the given URL
    ignoredHeaders.add("host");

    // Add -I (HEAD)
    // For servers that supports HEAD.
    // This will fetch the header of a document only.
    if (data.method === "HEAD") {
      addParam("-I");
    } else if (data.method !== "GET") {
      // Add method.
      // For HEAD and GET requests this is not necessary. GET is the
      // default, -I implies HEAD.
      addParam("-X");
      addParam(data.method);
    }

    // Add request headers.
    let headers = data.headers;
    if (multipartRequest) {
      const multipartHeaders = utils.getHeadersFromMultipartText(postDataText);
      headers = headers.concat(multipartHeaders);
    }
    for (let i = 0; i < headers.length; i++) {
      const header = headers[i];
      if (ignoredHeaders.has(header.name.toLowerCase())) {
        continue;
      }
      addParam("-H");
      addParam(header.name + ": " + header.value);
    }

    // Add post data.
    command = command.concat(postData);

    return command.join(" ");
  },
};

exports.Curl = Curl;

/**
 * Utility functions for the Curl command generator.
 */
const CurlUtils = {
  /**
   * Check if the request is an URL encoded request.
   *
   * @param object data
   *        The data source. See the description in the Curl object.
   * @return boolean
   *         True if the request is URL encoded, false otherwise.
   */
  isUrlEncodedRequest: function(data) {
    let postDataText = data.postDataText;
    if (!postDataText) {
      return false;
    }

    postDataText = postDataText.toLowerCase();
    if (
      postDataText.includes("content-type: application/x-www-form-urlencoded")
    ) {
      return true;
    }

    const contentType = this.findHeader(data.headers, "content-type");

    return (
      contentType &&
      contentType.toLowerCase().includes("application/x-www-form-urlencoded")
    );
  },

  /**
   * Check if the request is a multipart request.
   *
   * @param object data
   *        The data source.
   * @return boolean
   *         True if the request is multipart reqeust, false otherwise.
   */
  isMultipartRequest: function(data) {
    let postDataText = data.postDataText;
    if (!postDataText) {
      return false;
    }

    postDataText = postDataText.toLowerCase();
    if (postDataText.includes("content-type: multipart/form-data")) {
      return true;
    }

    const contentType = this.findHeader(data.headers, "content-type");

    return (
      contentType && contentType.toLowerCase().includes("multipart/form-data;")
    );
  },

  /**
   * Write out paramters from post data text.
   *
   * @param object postDataText
   *        Post data text.
   * @return string
   *         Post data parameters.
   */
  writePostDataTextParams: function(postDataText) {
    if (!postDataText) {
      return "";
    }
    const lines = postDataText.split("\r\n");
    return lines[lines.length - 1];
  },

  /**
   * Finds the header with the given name in the headers array.
   *
   * @param array headers
   *        Array of headers info {name:x, value:x}.
   * @param string name
   *        The header name to find.
   * @return string
   *         The found header value or null if not found.
   */
  findHeader: function(headers, name) {
    if (!headers) {
      return null;
    }

    name = name.toLowerCase();
    for (const header of headers) {
      if (name == header.name.toLowerCase()) {
        return header.value;
      }
    }

    return null;
  },

  /**
   * Returns the boundary string for a multipart request.
   *
   * @param string data
   *        The data source. See the description in the Curl object.
   * @return string
   *         The boundary string for the request.
   */
  getMultipartBoundary: function(data) {
    const boundaryRe = /\bboundary=(-{3,}\w+)/i;

    // Get the boundary string from the Content-Type request header.
    const contentType = this.findHeader(data.headers, "Content-Type");
    if (boundaryRe.test(contentType)) {
      return contentType.match(boundaryRe)[1];
    }
    // Temporary workaround. As of 2014-03-11 the requestHeaders array does not
    // always contain the Content-Type header for mulitpart requests. See bug 978144.
    // Find the header from the request payload.
    const boundaryString = data.postDataText.match(boundaryRe)[1];
    if (boundaryString) {
      return boundaryString;
    }

    return null;
  },

  /**
   * Removes the binary data from multipart text.
   *
   * @param string multipartText
   *        Multipart form data text.
   * @param string boundary
   *        The boundary string.
   * @return string
   *         The multipart text without the binary data.
   */
  removeBinaryDataFromMultipartText: function(multipartText, boundary) {
    let result = "";
    boundary = "--" + boundary;
    const parts = multipartText.split(boundary);
    for (const part of parts) {
      // Each part is expected to have a content disposition line.
      let contentDispositionLine = part.trimLeft().split("\r\n")[0];
      if (!contentDispositionLine) {
        continue;
      }
      contentDispositionLine = contentDispositionLine.toLowerCase();
      if (contentDispositionLine.includes("content-disposition: form-data")) {
        if (contentDispositionLine.includes("filename=")) {
          // The header lines and the binary blob is separated by 2 CRLF's.
          // Add only the headers to the result.
          const headers = part.split("\r\n\r\n")[0];
          result += boundary + headers + "\r\n\r\n";
        } else {
          result += boundary + part;
        }
      }
    }
    result += boundary + "--\r\n";

    return result;
  },

  /**
   * Get the headers from a multipart post data text.
   *
   * @param string multipartText
   *        Multipart post text.
   * @return array
   *         An array of header objects {name:x, value:x}
   */
  getHeadersFromMultipartText: function(multipartText) {
    const headers = [];
    if (!multipartText || multipartText.startsWith("---")) {
      return headers;
    }

    // Get the header section.
    const index = multipartText.indexOf("\r\n\r\n");
    if (index == -1) {
      return headers;
    }

    // Parse the header lines.
    const headersText = multipartText.substring(0, index);
    const headerLines = headersText.split("\r\n");
    let lastHeaderName = null;

    for (const line of headerLines) {
      // Create a header for each line in fields that spans across multiple lines.
      // Subsquent lines always begins with at least one space or tab character.
      // (rfc2616)
      if (lastHeaderName && /^\s+/.test(line)) {
        headers.push({ name: lastHeaderName, value: line.trim() });
        continue;
      }

      const indexOfColon = line.indexOf(":");
      if (indexOfColon == -1) {
        continue;
      }

      const header = [
        line.slice(0, indexOfColon),
        line.slice(indexOfColon + 1),
      ];
      if (header.length != 2) {
        continue;
      }
      lastHeaderName = header[0].trim();
      headers.push({ name: lastHeaderName, value: header[1].trim() });
    }

    return headers;
  },

  /**
   * Escape util function for POSIX oriented operating systems.
   * Credit: Google DevTools
   */
  escapeStringPosix: function(str) {
    function escapeCharacter(x) {
      let code = x.charCodeAt(0);
      if (code < 256) {
        // Add leading zero when needed to not care about the next character.
        return code < 16
          ? "\\x0" + code.toString(16)
          : "\\x" + code.toString(16);
      }
      code = code.toString(16);
      return "\\u" + ("0000" + code).substr(code.length, 4);
    }

    if (/[^\x20-\x7E]|\'/.test(str)) {
      // Use ANSI-C quoting syntax.
      return (
        "$'" +
        str
          .replace(/\\/g, "\\\\")
          .replace(/\'/g, "\\'")
          .replace(/\n/g, "\\n")
          .replace(/\r/g, "\\r")
          .replace(/!/g, "\\041")
          .replace(/[^\x20-\x7E]/g, escapeCharacter) +
        "'"
      );
    }

    // Use single quote syntax.
    return "'" + str + "'";
  },

  /**
   * Escape util function for Windows systems.
   * Credit: Google DevTools
   */
  escapeStringWin: function(str) {
    /*
       Replace the backtick character ` with `` in order to escape it.
       The backtick character is an escape character in PowerShell and
       can, among other things, be used to disable the effect of some
       of the other escapes created below.
       Also see http://www.rlmueller.net/PowerShellEscape.htm for
       useful details.

       Replace dollar sign because of commands in powershell when using
       double quotes. e.g $(calc.exe) Also see
       http://www.rlmueller.net/PowerShellEscape.htm for details.

       Replace quote by double quote (but not by \") because it is
       recognized by both cmd.exe and MS Crt arguments parser.

       Replace % by "%" because it could be expanded to an environment
       variable value. So %% becomes "%""%". Even if an env variable ""
       (2 doublequotes) is declared, the cmd.exe will not
       substitute it with its value.

       Replace each backslash with double backslash to make sure
       MS Crt arguments parser won't collapse them.

       Replace new line outside of quotes since cmd.exe doesn't let
       to do it inside. At the same time it gets duplicated,
       because first newline is consumed by ^.
       So for quote: `"Text-start\r\ntext-continue"`,
       we get: `"Text-start"^\r\n\r\n"text-continue"`,
       where `^\r\n` is just breaking the command, the `\r\n` right
       after is actual escaped newline.
    */
    return (
      '"' +
      str
        .replaceAll("`", "``")
        .replaceAll("$", "`$")
        .replaceAll('"', '""')
        .replaceAll("%", '"%"')
        .replace(/\\/g, "\\\\")
        .replace(/[\r\n]{1,2}/g, '"^$&$&"') +
      '"'
    );
  },
};

exports.CurlUtils = CurlUtils;
