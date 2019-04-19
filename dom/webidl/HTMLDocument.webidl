/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[OverrideBuiltins]
interface HTMLDocument : Document {
           [SetterThrows]
           attribute DOMString domain;
  // DOM tree accessors
  [Throws]
  getter object (DOMString name);

  // dynamic markup insertion
  [CEReactions, Throws]
  Document open(optional DOMString type, optional DOMString replace = ""); // type is ignored
  [CEReactions, Throws]
  WindowProxy? open(DOMString url, DOMString name, DOMString features, optional boolean replace = false);
  [CEReactions, Throws]
  void close();
  [CEReactions, Throws]
  void write(DOMString... text);
  [CEReactions, Throws]
  void writeln(DOMString... text);

  [CEReactions, SetterThrows, SetterNeedsSubjectPrincipal]
           attribute DOMString designMode;
  [CEReactions, Throws, NeedsSubjectPrincipal]
  boolean execCommand(DOMString commandId, optional boolean showUI = false,
                      optional DOMString value = "");
  [Throws, NeedsSubjectPrincipal]
  boolean queryCommandEnabled(DOMString commandId);
  [Throws]
  boolean queryCommandIndeterm(DOMString commandId);
  [Throws]
  boolean queryCommandState(DOMString commandId);
  [NeedsCallerType]
  boolean queryCommandSupported(DOMString commandId);
  [Throws]
  DOMString queryCommandValue(DOMString commandId);

  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString fgColor;
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString linkColor;
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString vlinkColor;
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString alinkColor;
  [CEReactions] attribute [TreatNullAs=EmptyString] DOMString bgColor;

  void clear();

  readonly attribute HTMLAllCollection all;

  // @deprecated These are old Netscape 4 methods. Do not use,
  //             the implementation is no-op.
  // XXXbz do we actually need these anymore?
  void                      captureEvents();
  void                      releaseEvents();
};

partial interface HTMLDocument {
  /*
   * Number of nodes that have been blocked by the Safebrowsing API to prevent
   * tracking, cryptomining and so on. This method is for testing only.
   */
  [ChromeOnly, Pure]
  readonly attribute long blockedNodeByClassifierCount;

  /*
   * List of nodes that have been blocked by the Safebrowsing API to prevent
   * tracking, fingerprinting, cryptomining and so on. This method is for
   * testing only.
   */
  [ChromeOnly, Pure]
  readonly attribute NodeList blockedNodesByClassifier;

  [ChromeOnly]
  void userInteractionForTesting();

  /**
   * setKeyPressEventModel() is called when we need to check whether the web
   * app requires specific keypress event model or not.
   *
   * @param aKeyPressEventModel  Proper keypress event model for the web app.
   *   KEYPRESS_EVENT_MODEL_DEFAULT:
   *     Use default keypress event model.  I.e., depending on
   *     "dom.keyboardevent.keypress.set_keycode_and_charcode_to_same_value"
   *     pref.
   *   KEYPRESS_EVENT_MODEL_SPLIT:
   *     Use split model.  I.e, if keypress event inputs a character,
   *     keyCode should be 0.  Otherwise, charCode should be 0.
   *   KEYPRESS_EVENT_MODEL_CONFLATED:
   *     Use conflated model.  I.e., keyCode and charCode values of each
   *     keypress event should be set to same value.
   */
  [ChromeOnly]
  const unsigned short KEYPRESS_EVENT_MODEL_DEFAULT = 0;
  [ChromeOnly]
  const unsigned short KEYPRESS_EVENT_MODEL_SPLIT = 1;
  [ChromeOnly]
  const unsigned short KEYPRESS_EVENT_MODEL_CONFLATED = 2;
  [ChromeOnly]
  void setKeyPressEventModel(unsigned short aKeyPressEventModel);
};
