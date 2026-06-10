/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Shared cookie utilities for browser automation services.
 * Eliminates duplicate cookie logic between WebScraperServices and TabManagerServices.
 */

/**
 * Cookie data structure matching CookieData from os-server/shared/types.ts
 */
interface CookieData {
  name: string;
  value: string;
  domain?: string;
  path?: string;
  secure?: boolean;
  httpOnly?: boolean;
  sameSite?: "Strict" | "Lax" | "None";
  expirationDate?: number;
}

export class CookieHelper {
  /**
   * Gets all cookies for the browser's current page.
   *
   * Uses dual originAttributes strategy (real + empty fallback) to find
   * cookies set with and without container/private-browsing attributes.
   */
  static getCookies(browser: XULElement): CookieData[] {
    try {
      const uri = browser.browsingContext?.currentURI;
      if (!uri) return [];

      const host = uri.host;
      const principal =
        browser.browsingContext?.currentWindowGlobal?.documentPrincipal;
      const originAttributes = principal?.originAttributes ?? {};

      const cookies: CookieData[] = [];
      const cookieManager = Services.cookies;
      const originAttrCandidates = [originAttributes, {}];

      const seen = new Set<string>();

      for (const attrs of originAttrCandidates) {
        const enumerator = cookieManager.getCookiesFromHost(host, attrs);

        for (const cookie of enumerator) {
          const key = `${cookie.name}\u0001${cookie.host}\u0001${cookie.path}`;
          if (seen.has(key)) continue;
          seen.add(key);

          cookies.push({
            name: cookie.name,
            value: cookie.value,
            domain: cookie.host,
            path: cookie.path,
            secure: cookie.isSecure,
            httpOnly: cookie.isHttpOnly,
            sameSite: ["None", "Lax", "Strict"][cookie.sameSite] ?? "None",
            expirationDate: cookie.expiry,
          });
        }
      }

      return cookies;
    } catch (e) {
      console.error("[CookieHelper] Error getting cookies:", e);
      return [];
    }
  }

  /**
   * Sets a cookie using the parent-process Services.cookies.add() API.
   * Tries both real and empty originAttributes for maximum compatibility.
   *
   * @returns true if successfully set via Services.cookies, false otherwise
   */
  static setCookieDirect(browser: XULElement, cookie: CookieData): boolean {
    try {
      const uri = browser.browsingContext?.currentURI;
      if (!uri) return false;

      const sameSiteMap: Record<string, number> = {
        None: 0,
        Lax: 1,
        Strict: 2,
      };

      const principal =
        browser.browsingContext?.currentWindowGlobal?.documentPrincipal;
      const originAttributes = principal?.originAttributes ?? {};

      const isSecure = cookie.secure ?? uri.schemeIs("https");
      let requestedSameSite = cookie.sameSite ?? "Lax";
      if (requestedSameSite === "None" && !isSecure) {
        // Firefox requires Secure for SameSite=None; fall back to Lax on HTTP
        requestedSameSite = "Lax";
      }
      const schemeMap = isSecure
        ? Ci.nsICookie.SCHEME_HTTPS
        : uri.schemeIs("http")
          ? Ci.nsICookie.SCHEME_HTTP
          : 0; // SCHEME_UNKNOWN

      const attrsList = [originAttributes];
      if (Object.keys(originAttributes).length > 0) {
        attrsList.push({});
      }

      for (const attrs of attrsList) {
        try {
          const validation = Services.cookies.add(
            cookie.domain ?? uri.host,
            cookie.path ?? "/",
            cookie.name,
            cookie.value,
            isSecure,
            cookie.httpOnly ?? false,
            false, // isSession
            cookie.expirationDate ??
              Math.floor(Date.now() / 1000) + 86400 * 365,
            attrs,
            sameSiteMap[requestedSameSite] ?? sameSiteMap.Lax,
            schemeMap,
            Boolean(
              (attrs as unknown as { partitionKey?: unknown }).partitionKey,
            ),
          );

          if (validation?.result !== Ci.nsICookieValidation.eOK) {
            console.error(
              "[CookieHelper] Cookie rejected:",
              validation?.result,
              validation?.errorString,
            );
            continue;
          }

          // Validation passed — trust the result (cookieExists may return false
          // immediately after add() due to timing or originAttributes mismatch)
          return true;
        } catch (err) {
          console.error("[CookieHelper] Error adding cookie:", err);
        }
      }

      return false;
    } catch (e) {
      console.error("[CookieHelper] Error in setCookieDirect:", e);
      return false;
    }
  }

  /**
   * Builds a Set-Cookie style string for actor-based (content-process) fallback.
   */
  static buildCookieString(cookie: CookieData, host: string): string {
    const parts = [
      `${cookie.name}=${cookie.value}`,
      `Path=${cookie.path ?? "/"}`,
      `SameSite=${cookie.sameSite ?? "Lax"}`,
    ];
    const domain = cookie.domain ?? host;
    if (domain) parts.push(`Domain=${domain}`);
    if (cookie.expirationDate) {
      parts.push(
        `Expires=${new Date(cookie.expirationDate * 1000).toUTCString()}`,
      );
    }
    if (cookie.secure) parts.push("Secure");
    return parts.filter(Boolean).join("; ");
  }
}
