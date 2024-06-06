/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Bug 1897120 - Override "MozTransform" in element.style and return false
 * Webcompat issue #137038 - https://github.com/webcompat/web-bugs/issues/137038
 *
 * The site is using turn js and detecting "MozTransform" in document.body.style,
 * which returns true at the moment and uses the -moz-transform, which doesn't work,
 * so zooming functionality breaks. Overriding "MozTransform" in element.style to
 * return false fixes the problem.
 */

/* globals exportFunction, cloneInto */

console.info(
  "Overriding MozTransform in element.style for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1897120 for details."
);

(function () {
  const ele = HTMLElement.wrappedJSObject.prototype;
  const obj = window.wrappedJSObject.Object;
  const style = obj.getOwnPropertyDescriptor(ele, "style");
  const { get } = style;
  style.get = exportFunction(function () {
    const styles = get.call(this);
    return new window.wrappedJSObject.Proxy(
      styles,
      cloneInto(
        {
          deleteProperty(target, prop) {
            return Reflect.deleteProperty(target, prop);
          },
          get(target, key) {
            const val = Reflect.get(target, key);
            if (typeof val == "function") {
              // We can't just return the function, as it's a method which
              // needs `this` to be the styles object. So we return a wrapper.
              return exportFunction(function () {
                return val.apply(styles, arguments);
              }, window);
            }
            return val;
          },
          has(target, key) {
            if (key == "MozTransform" || key == "WebkitTransform") {
              return false;
            }
            return Reflect.has(target, key);
          },
          ownKeys(target) {
            return Reflect.ownKeys(target);
          },
          set(target, key, value) {
            return Reflect.set(target, key, value);
          },
        },
        window,
        { cloneFunctions: true }
      )
    );
  }, window);
  obj.defineProperty(ele, "style", style);
})();
