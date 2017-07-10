/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BrowserTestUtils",
  "resource://testing-common/BrowserTestUtils.jsm");
