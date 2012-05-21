/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  const C_i = Components.interfaces;
  const nsIXTFElementFactory = C_i.nsIXTFElementFactory;
  const nsIXTFPrivate        = C_i.nsIXTFPrivate;
  const nsIDOMParser         = C_i.nsIDOMParser;
  const nsIDOMEventTarget    = C_i.nsIDOMEventTarget;
  do_load_manifest("xtfComponent.manifest");
  const xtfClass = "@mozilla.org/xtf/element-factory;1?namespace=";

  do_check_true(xtfClass + "xtf-tests;foo" in Components.classes);
  var fooFactory = Components.classes[xtfClass + "xtf-tests;foo"]
                             .getService(nsIXTFElementFactory);
  do_check_true(Boolean(fooFactory));

  var xmlSource = "<bar xmlns='xtf-tests;foo'/>";
  const parser     = Components.classes["@mozilla.org/xmlextras/domparser;1"]
                               .createInstance(nsIDOMParser);
  var xmlDoc = parser.parseFromString(xmlSource, "application/xml");
  do_check_true(xmlDoc.documentElement instanceof nsIXTFPrivate);
  do_check_true(xmlDoc.documentElement instanceof nsIDOMEventTarget);

  do_check_true(xmlDoc.documentElement.inner.wrappedJSObject.testpassed);

  // Bug 378247
  xmlDoc.documentElement.addEventListener("DOMNodeInserted",
    function foo() {
      dump('This is a DOMNodeInserted test.\n');
    },
    true);
  xmlDoc.documentElement.appendChild(xmlDoc.createTextNode("bar"));
  do_check_true(xmlDoc.documentElement.inner.wrappedJSObject.testpassed);

  xmlSource = "<handle_default xmlns='xtf-tests;foo'/>";
  xmlDoc = parser.parseFromString(xmlSource, "application/xml");
  do_check_true(xmlDoc.documentElement instanceof nsIXTFPrivate);
  do_check_true(xmlDoc.documentElement instanceof nsIDOMEventTarget);

  // Bug 378247
  xmlDoc.documentElement.addEventListener("DOMNodeInserted",
    function foo() {
      dump('This is a DOMNodeInserted test.\n');
    },
    true);
  xmlDoc.documentElement.appendChild(xmlDoc.createTextNode("bar"));
  do_check_true(xmlDoc.documentElement.inner.wrappedJSObject.testpassed);
}
