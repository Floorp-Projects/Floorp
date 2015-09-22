/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cc } = require("chrome");
const l10n = require("gcli/l10n");
const URL = require("sdk/url").URL;

XPCOMUtils.defineLazyGetter(this, "cookieMgr", function() {
  const { Cc, Ci } = require("chrome");
  return Cc["@mozilla.org/cookiemanager;1"].getService(Ci.nsICookieManager2);
});

/**
 * Check host value and remove port part as it is not used
 * for storing cookies.
 *
 * Parameter will usually be context.environment.document.location.host
 */
function sanitizeHost(host) {
  if (host == null || host == "") {
    throw new Error(l10n.lookup("cookieListOutNonePage"));
  }
  return host.split(':')[0];
}

/**
 * The cookie 'expires' value needs converting into something more readable
 */
function translateExpires(expires) {
  if (expires == 0) {
    return l10n.lookup("cookieListOutSession");
  }
  return new Date(expires).toLocaleString();
}

/**
 * Check if a given cookie matches a given host
 */
function isCookieAtHost(cookie, host) {
  if (cookie.host == null) {
    return host == null;
  }
  if (cookie.host.startsWith(".")) {
    return host.endsWith(cookie.host);
  }
  else {
    return cookie.host == host;
  }
}

exports.items = [
  {
    name: "cookie",
    description: l10n.lookup("cookieDesc"),
    manual: l10n.lookup("cookieManual")
  },
  {
    item: "command",
    runAt: "server",
    name: "cookie list",
    description: l10n.lookup("cookieListDesc"),
    manual: l10n.lookup("cookieListManual"),
    returnType: "cookies",
    exec: function(args, context) {
      let host = sanitizeHost(context.environment.document.location.host);

      let enm = cookieMgr.getCookiesFromHost(host);

      let cookies = [];
      while (enm.hasMoreElements()) {
        let cookie = enm.getNext().QueryInterface(Ci.nsICookie);
        if (isCookieAtHost(cookie, host)) {
          cookies.push({
            host: cookie.host,
            name: cookie.name,
            value: cookie.value,
            path: cookie.path,
            expires: cookie.expires,
            secure: cookie.secure,
            httpOnly: cookie.httpOnly,
            sameDomain: cookie.sameDomain
          });
        }
      }

      return cookies;
    }
  },
  {
    item: "command",
    runAt: "server",
    name: "cookie remove",
    description: l10n.lookup("cookieRemoveDesc"),
    manual: l10n.lookup("cookieRemoveManual"),
    params: [
      {
        name: "name",
        type: "string",
        description: l10n.lookup("cookieRemoveKeyDesc"),
      }
    ],
    exec: function(args, context) {
      let host = sanitizeHost(context.environment.document.location.host);
      let enm = cookieMgr.getCookiesFromHost(host);

      let cookies = [];
      while (enm.hasMoreElements()) {
        let cookie = enm.getNext().QueryInterface(Ci.nsICookie);
        if (isCookieAtHost(cookie, host)) {
          if (cookie.name == args.name) {
            cookieMgr.remove(cookie.host, cookie.name, cookie.path, false);
          }
        }
      }
    }
  },
  {
    item: "converter",
    from: "cookies",
    to: "view",
    exec: function(cookies, context) {
      if (cookies.length == 0) {
        let host = new URL(context.environment.target.url).host;
        host = sanitizeHost(host);
        let msg = l10n.lookupFormat("cookieListOutNoneHost", [ host ]);
        return context.createView({ html: "<span>" + msg + "</span>" });
      }

      for (let cookie of cookies) {
        cookie.expires = translateExpires(cookie.expires);

        let noAttrs = !cookie.secure && !cookie.httpOnly && !cookie.sameDomain;
        cookie.attrs = (cookie.secure ? "secure" : " ") +
                       (cookie.httpOnly ? "httpOnly" : " ") +
                       (cookie.sameDomain ? "sameDomain" : " ") +
                       (noAttrs ? l10n.lookup("cookieListOutNone") : " ");
      }

      return context.createView({
        html:
          "<ul class='gcli-cookielist-list'>" +
          "  <li foreach='cookie in ${cookies}'>" +
          "    <div>${cookie.name}=${cookie.value}</div>" +
          "    <table class='gcli-cookielist-detail'>" +
          "      <tr>" +
          "        <td>" + l10n.lookup("cookieListOutHost") + "</td>" +
          "        <td>${cookie.host}</td>" +
          "      </tr>" +
          "      <tr>" +
          "        <td>" + l10n.lookup("cookieListOutPath") + "</td>" +
          "        <td>${cookie.path}</td>" +
          "      </tr>" +
          "      <tr>" +
          "        <td>" + l10n.lookup("cookieListOutExpires") + "</td>" +
          "        <td>${cookie.expires}</td>" +
          "      </tr>" +
          "      <tr>" +
          "        <td>" + l10n.lookup("cookieListOutAttributes") + "</td>" +
          "        <td>${cookie.attrs}</td>" +
          "      </tr>" +
          "      <tr><td colspan='2'>" +
          "        <span class='gcli-out-shortcut' onclick='${onclick}'" +
          "            data-command='cookie set ${cookie.name} '" +
          "            >" + l10n.lookup("cookieListOutEdit") + "</span>" +
          "        <span class='gcli-out-shortcut'" +
          "            onclick='${onclick}' ondblclick='${ondblclick}'" +
          "            data-command='cookie remove ${cookie.name}'" +
          "            >" + l10n.lookup("cookieListOutRemove") + "</span>" +
          "      </td></tr>" +
          "    </table>" +
          "  </li>" +
          "</ul>",
        data: {
          options: { allowEval: true },
          cookies: cookies,
          onclick: context.update,
          ondblclick: context.updateExec
        }
      });
    }
  },
  {
    item: "command",
    runAt: "server",
    name: "cookie set",
    description: l10n.lookup("cookieSetDesc"),
    manual: l10n.lookup("cookieSetManual"),
    params: [
      {
        name: "name",
        type: "string",
        description: l10n.lookup("cookieSetKeyDesc")
      },
      {
        name: "value",
        type: "string",
        description: l10n.lookup("cookieSetValueDesc")
      },
      {
        group: l10n.lookup("cookieSetOptionsDesc"),
        params: [
          {
            name: "path",
            type: { name: "string", allowBlank: true },
            defaultValue: "/",
            description: l10n.lookup("cookieSetPathDesc")
          },
          {
            name: "domain",
            type: "string",
            defaultValue: null,
            description: l10n.lookup("cookieSetDomainDesc")
          },
          {
            name: "secure",
            type: "boolean",
            description: l10n.lookup("cookieSetSecureDesc")
          },
          {
            name: "httpOnly",
            type: "boolean",
            description: l10n.lookup("cookieSetHttpOnlyDesc")
          },
          {
            name: "session",
            type: "boolean",
            description: l10n.lookup("cookieSetSessionDesc")
          },
          {
            name: "expires",
            type: "string",
            defaultValue: "Jan 17, 2038",
            description: l10n.lookup("cookieSetExpiresDesc")
          },
        ]
      }
    ],
    exec: function(args, context) {
      let host = sanitizeHost(context.environment.document.location.host);
      let time = Date.parse(args.expires) / 1000;

      cookieMgr.add(args.domain ? "." + args.domain : host,
                    args.path ? args.path : "/",
                    args.name,
                    args.value,
                    args.secure,
                    args.httpOnly,
                    args.session,
                    time);
    }
  }
];
