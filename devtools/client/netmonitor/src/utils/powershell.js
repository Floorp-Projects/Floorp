/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Copyright (C) 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Anthony Ricaud <rik@webkit.org>
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2022 Mozilla Foundation. All rights reserved.
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

// Utility to generate commands to invoke a request for powershell
"use strict";

// Some of these headers are passed in as seperate `Invoke-WebRequest` parameters so ignore
// when building the headers list, others are not to neccesarily restrict the request.
const IGNORED_HEADERS = [
  "connection",
  "proxy-connection",
  "content-length",
  "expect",
  "range",
  "host",
  "content-type",
  "user-agent",
  "cookie",
];
/**
 * This escapes strings for the powershell command
 *
 * 1. Escape the backtick, dollar sign and the double quotes See https://www.rlmueller.net/PowerShellEscape.htm
 * 2. Convert any non printing ASCII characters found, using the ASCII code.
 */
function escapeStr(str) {
  return `"${str
    .replace(/[`\$"]/g, "`$&")
    .replace(/[^\x20-\x7E]/g, char => "$([char]" + char.charCodeAt(0) + ")")}"`;
}

const PowerShell = {
  generateCommand(url, method, headers, postData, cookies) {
    const parameters = [];

    // Create a WebSession to pass the information about cookies
    // See https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.utility/invoke-webrequest?view=powershell-7.2#-websession
    const session = [];
    for (const { name, value, domain } of cookies) {
      if (!session.length) {
        session.push(
          "$session = New-Object Microsoft.PowerShell.Commands.WebRequestSession"
        );
      }
      session.push(
        `$session.Cookies.Add((New-Object System.Net.Cookie(${escapeStr(
          name
        )}, ${escapeStr(value)}, "/", ${escapeStr(
          domain || new URL(url).host
        )})))`
      );
    }

    parameters.push(`-Uri ${escapeStr(url)}`);

    if (method !== "GET") {
      parameters.push(`-Method ${method}`);
    }

    if (session.length) {
      parameters.push("-WebSession $session");
    }

    const userAgent = headers.find(
      ({ name }) => name.toLowerCase() === "user-agent"
    );
    if (userAgent) {
      parameters.push("-UserAgent " + escapeStr(userAgent.value));
    }

    const headersStr = [];
    for (let { name, value } of headers) {
      // Translate any HTTP2 pseudo headers to HTTP headers
      name = name.replace(/^:/, "");

      if (IGNORED_HEADERS.includes(name.toLowerCase())) {
        continue;
      }
      headersStr.push(`${escapeStr(name)} = ${escapeStr(value)}`);
    }
    if (headersStr.length) {
      parameters.push(`-Headers @{\n${headersStr.join("\n  ")}\n}`);
    }

    const contentType = headers.find(
      header => header.name.toLowerCase() === "content-type"
    );
    if (contentType) {
      parameters.push("-ContentType " + escapeStr(contentType.value));
    }

    const formData = postData.text;
    if (formData) {
      // Encode bytes if any of the characters is not an ASCII printing character (not between Space character and ~ character)
      // a-zA-Z0-9 etc. See http://www.asciitable.com/
      const body = /[^\x20-\x7E]/.test(formData)
        ? "([System.Text.Encoding]::UTF8.GetBytes(" + escapeStr(formData) + "))"
        : escapeStr(formData);
      parameters.push("-Body " + body);
    }

    return `${
      session.length ? session.join("\n").concat("\n") : ""
      // -UseBasicParsing is added for backward compatibility.
      // See https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.utility/invoke-webrequest?view=powershell-7.2#-usebasicparsing
    }Invoke-WebRequest -UseBasicParsing ${parameters.join(" `\n")}`;
  },
};

exports.PowerShell = PowerShell;
