/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Debugger.Source objects have a `url` property that exposes the value
 * that was passed to SpiderMonkey, but unfortunately often SpiderMonkey
 * sets a URL even in cases where it doesn't make sense, so we have to
 * explicitly ignore the URL value in these contexts to keep things a bit
 * more consistent.
 *
 * @param {Debugger.Source} source
 *
 * @return {string | null}
 */
function getDebuggerSourceURL(source) {
  const introType = source.introductionType;

  // These are all the sources that are eval or eval-like, but may still have
  // a URL set on the source, so we explicitly ignore the source URL for these.
  if (
    introType === "injectedScript" ||
    introType === "eval" ||
    introType === "debugger eval" ||
    introType === "Function" ||
    introType === "javascriptURL" ||
    introType === "eventHandler" ||
    introType === "domTimer"
  ) {
    return null;
  }
  // When using <iframe srcdoc="<script> js source </script>"/>, we can't easily fetch the srcdoc
  // full html text content. So, consider each inline script as independant source with
  // their own URL. Thus the ID appended to each URL.
  if (source.url == "about:srcdoc") {
    return source.url + "#" + source.id;
  }

  return source.url;
}

exports.getDebuggerSourceURL = getDebuggerSourceURL;
