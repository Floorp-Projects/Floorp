// SPDX-License-Identifier: MPL-2.0

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export function disableCspInDevPlugin(isDev: boolean) {
  if (!isDev) {
    return {name:"disable-csp-in-dev"};
  }

  return {
    name: "disable-csp-in-dev",
    enforce: "post",
    transformIndexHtml(html: string) {
      return html.replace(
        /<meta http-equiv="Content-Security-Policy" content="[^"]*">/i,
        "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src * 'unsafe-inline' 'unsafe-eval' data: blob:;\">",
      );
    },
  };
}
