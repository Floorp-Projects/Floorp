/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * <https://w3c.github.io/trusted-types/dist/spec/>.
 * It is augmented with Gecko-specific annotations.
 */

[Exposed=(Window,Worker), Pref="dom.security.trusted_types.enabled"]
interface TrustedHTML {
  stringifier;
  DOMString toJSON();
};

[Exposed=(Window,Worker), Pref="dom.security.trusted_types.enabled"]
interface TrustedScript {
  stringifier;
  DOMString toJSON();
};

[Exposed=(Window,Worker), Pref="dom.security.trusted_types.enabled"]
interface TrustedScriptURL {
  stringifier;
  USVString toJSON();
};

[Exposed=(Window,Worker), Pref="dom.security.trusted_types.enabled"]
interface TrustedTypePolicy {
  readonly attribute DOMString name;
  [NewObject, Throws] TrustedHTML createHTML(DOMString input, any... arguments);
  [NewObject, Throws] TrustedScript createScript(DOMString input, any... arguments);
  [NewObject, Throws] TrustedScriptURL createScriptURL(DOMString input, any... arguments);
};

dictionary TrustedTypePolicyOptions {
   CreateHTMLCallback createHTML;
   CreateScriptCallback createScript;
   CreateScriptURLCallback createScriptURL;
};

callback CreateHTMLCallback = DOMString? (DOMString input, any... arguments);
callback CreateScriptCallback = DOMString? (DOMString input, any... arguments);
callback CreateScriptURLCallback = USVString? (DOMString input, any... arguments);

[Exposed=(Window,Worker), Pref="dom.security.trusted_types.enabled"]
interface TrustedTypePolicyFactory {
    TrustedTypePolicy createPolicy(DOMString policyName , optional TrustedTypePolicyOptions policyOptions = {});
    boolean isHTML(any value);
    boolean isScript(any value);
    boolean isScriptURL(any value);
    [Pure, StoreInSlot] readonly attribute TrustedHTML emptyHTML;
    [Pure, StoreInSlot] readonly attribute TrustedScript emptyScript;
    DOMString? getAttributeType(
      DOMString tagName,
      DOMString attribute,
      optional DOMString elementNs = "",
      optional DOMString attrNs = "");
    DOMString? getPropertyType(
        DOMString tagName,
        DOMString property,
        optional DOMString elementNs = "");
    readonly attribute TrustedTypePolicy? defaultPolicy;
};
