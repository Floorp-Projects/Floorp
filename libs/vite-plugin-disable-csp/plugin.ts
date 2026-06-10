// SPDX-License-Identifier: MPL-2.0

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export function disableCspInDevPlugin(isDev: boolean) {
  if (!isDev) {
    return [];
  }

  return {
    name: "disable-csp-in-dev",
    enforce: "post",
    transformIndexHtml(html: string, ctx) {
      // Replace restrictive CSP with a permissive one for dev mode
      let transformed = html.replace(
        /<meta http-equiv="Content-Security-Policy" content="[^"]*">/i,
        "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src * 'unsafe-inline' 'unsafe-eval' data: blob:;\">",
      );

      // When loaded via CustomAboutPage (about:welcome → localhost),
      // relative URLs like "/@vite/client" resolve to "about:/@vite/client".
      // Convert all relative URLs to absolute URLs using the dev server origin.
      const port = ctx.server?.config?.server?.port;
      if (port) {
        const origin = `http://localhost:${port}`;
        // Fix <script src="/..."> attributes
        transformed = transformed.replace(
          /(<script[^>]*\bsrc=")\/(?!\/)([^"]*")/g,
          `$1${origin}/$2`,
        );
        // Fix <link href="/..."> attributes
        transformed = transformed.replace(
          /(<link[^>]*\bhref=")\/(?!\/)([^"]*")/g,
          `$1${origin}/$2`,
        );
        // Fix inline import statements: from "/@react-refresh" etc.
        transformed = transformed.replace(
          /(from\s*["'])\/(?!\/)([^"']*["'])/g,
          `$1${origin}/$2`,
        );
      }

      return transformed;
    },
  };
}
